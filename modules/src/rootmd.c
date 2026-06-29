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
    // This is a simplified main loop for the rootmd module
    // In a real scenario, this would be a task that blocks on a syscall
    // waiting for IPC messages.
    
    while (1) {
        uint64_t sender_pid;
        uint64_t data[4]; // Buffer for IPC data
        
        // Assume a syscall like syscall_recv(pid, &sender, data) exists
        // For the sake of this implementation, we simulate the logic:
        // int received = syscall(SYSCALL_RECV, ROOTMD_PID, &sender_pid, data);
        
        // Simulation of processing a message:
        uint64_t command = data[0];
        
        if (command == ROOTMD_REGISTER) {
            uint64_t mod_pid = data[1];
            int slot = get_free_slot();
            if (slot != -1) {
                registered_mods[slot].pid = mod_pid;
                registered_mods[slot].active = 1;
                // In a real system, we would copy the name from the IPC data
            }
        } else if (command == ROOTMD_UNREGISTER) {
            uint64_t mod_pid = data[1];
            int slot = find_mod_slot(mod_pid);
            if (slot != -1) {
                registered_mods[slot].active = 0;
            }
        } else if (command == ROOTMD_LIST) {
            // Logic to iterate through registered_mods and send a list back to sender
        }
    }
}
