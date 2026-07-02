#ifndef MODMAN_H
#define MODMAN_H

#include <stdint.h>

#define MODMAN_MAX_MODS 64
#define MODMAN_NAME_MAX 64

// Syscall-based commands (used by user-space via SYSCALL_MOD_*)
#define MODMAN_REGISTER   1
#define MODMAN_UNREGISTER 2
#define MODMAN_LIST       3
#define MODMAN_LOOKUP     4
#define MODMAN_LOOKUP_PID 5

typedef struct {
    uint64_t pid;
    char name[MODMAN_NAME_MAX];
} ModEntry;

void modman_init(void);
int  modman_register(uint64_t pid, const char* name);
int  modman_unregister(uint64_t pid);
int  modman_lookup(const char* name, uint64_t* out_pid);
int  modman_lookup_pid(uint64_t pid, char* out_name);
int  modman_list(ModEntry* entries, int max);
int  modman_register_embedded(uint64_t pid, void* data, uint64_t size, uint64_t* out_entry_off);
int modman_unregister_current();

#endif
