#include "rootmd.h"
#include "module.h"
#include <stdint.h>
#include <stddef.h>

// Simple internal storage for registered modules
typedef struct {
    uint64_t pid;
    char name[MOD_NAME_MAX];
    int active;
} RegisteredMod;

#define MAX_REGISTERED_MODS 64
static RegisteredMod registered_mods[MAX_REGISTERED_MODS];

static int find_mod_slot(uint64_t pid) {
    for (int i = 0; i < MAX_REGISTERED_MODS; i++) {
        if (registered_mods[i].active && registered_mods[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

static int get_free_slot() {
    for (int i = 0; i < MAX_REGISTERED_MODS; i++) {
        if (!registered_mods[i].active) {
            return i;
        }
    }
    return -1;
}

void rootmd_main() {
    while (1) {
        unsigned char buf[64];
        unsigned long long sender = recv(buf);
        if (sender == (unsigned long long)-1) {
            continue;
        }

        uint64_t *msg = (uint64_t *)buf;
        uint64_t command = msg[0];

        if (command == ROOTMD_REGISTER) {
            // data: pid (uint64_t) at offset 8, name string after that
            uint64_t mod_pid = msg[1];
            char *name_ptr = (char *)(buf + 8 + 8);
            int slot = find_mod_slot(mod_pid);
            if (slot == -1) {
                slot = get_free_slot();
            }
            if (slot != -1) {
                registered_mods[slot].pid = mod_pid;
                registered_mods[slot].active = 1;
                for (int i = 0; i < MOD_NAME_MAX && name_ptr[i] != '\0'; i++) {
                    registered_mods[slot].name[i] = name_ptr[i];
                }
                registered_mods[slot].name[MOD_NAME_MAX - 1] = '\0';
            }
        } else if (command == ROOTMD_UNREGISTER) {
            uint64_t mod_pid = msg[1];
            int slot = find_mod_slot(mod_pid);
            if (slot != -1) {
                registered_mods[slot].active = 0;
            }
        } else if (command == ROOTMD_LIST) {
            uint64_t active_count = 0;
            for (int i = 0; i < MAX_REGISTERED_MODS; i++) {
                if (registered_mods[i].active) {
                    active_count++;
                }
            }
            uint64_t response = active_count;
            send(sender, &response);
        }
    }
}
