#include "modman.h"
#include "memory/memutils.h"
#include "debug.h"

typedef struct {
    uint64_t pid;
    char name[MODMAN_NAME_MAX];
    int active;
} ModEntryInternal;

static ModEntryInternal mods[MODMAN_MAX_MODS];
static int initialized = 0;

void modman_init(void) {
    for (int i = 0; i < MODMAN_MAX_MODS; i++)
        mods[i].active = 0;
    initialized = 1;
    DEBUG_INFO("MODMAN: module registry initialized");
}

static int find_by_pid(uint64_t pid) {
    for (int i = 0; i < MODMAN_MAX_MODS; i++) {
        if (mods[i].active && mods[i].pid == pid)
            return i;
    }
    return -1;
}

static int find_by_name(const char* name) {
    for (int i = 0; i < MODMAN_MAX_MODS; i++) {
        if (!mods[i].active) continue;
        int match = 1;
        for (int j = 0; j < MODMAN_NAME_MAX; j++) {
            if (mods[i].name[j] != name[j]) { match = 0; break; }
            if (name[j] == '\0') break;
        }
        if (match) return i;
    }
    return -1;
}

static int free_slot(void) {
    for (int i = 0; i < MODMAN_MAX_MODS; i++) {
        if (!mods[i].active) return i;
    }
    return -1;
}

static void copy_name(char* dst, const char* src) {
    for (int i = 0; i < MODMAN_NAME_MAX - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[MODMAN_NAME_MAX - 1] = '\0';
}

int modman_register(uint64_t pid, const char* name) {
    if (!initialized) return -1;
    int slot = find_by_pid(pid);
    if (slot == -1) slot = free_slot();
    if (slot == -1) return -1;
    mods[slot].pid = pid;
    mods[slot].active = 1;
    copy_name(mods[slot].name, name);
    DEBUG_INFO("MODMAN: registered pid=%lu name='%s'", pid, name);
    return 0;
}

int modman_unregister(uint64_t pid) {
    if (!initialized) return -1;
    int slot = find_by_pid(pid);
    if (slot == -1) return -1;
    mods[slot].active = 0;
    DEBUG_INFO("MODMAN: unregistered pid=%lu", pid);
    return 0;
}

int modman_lookup(const char* name, uint64_t* out_pid) {
    if (!initialized) return -1;
    int slot = find_by_name(name);
    if (slot == -1) return -1;
    if (out_pid) *out_pid = mods[slot].pid;
    return 0;
}

int modman_lookup_pid(uint64_t pid, char* out_name) {
    if (!initialized) return -1;
    int slot = find_by_pid(pid);
    if (slot == -1) return -1;
    if (out_name) {
        for (int i = 0; i < MODMAN_NAME_MAX; i++)
            out_name[i] = mods[slot].name[i];
    }
    return 0;
}

int modman_list(ModEntry* entries, int max) {
    if (!initialized) return -1;
    int count = 0;
    for (int i = 0; i < MODMAN_MAX_MODS && count < max; i++) {
        if (mods[i].active) {
            entries[count].pid = mods[i].pid;
            for (int j = 0; j < MODMAN_NAME_MAX; j++)
                entries[count].name[j] = mods[i].name[j];
            count++;
        }
    }
    return count;
}
