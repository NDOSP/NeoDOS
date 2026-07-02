#ifndef MODSTD_H
#define MODSTD_H

#include "modlib.h"
#include "std.h"

#define REGISTER_MODULE(name) \
    MODINFO(name); \
    \
    extern void init(void); \
    extern void loop(void); \
    \
    __attribute__((section(".text.start"))) \
    void _start(void) { \
        debug_puts(name); \
        debug_puts(": init"); \
        debug_puts("\n"); \
        \
        init(); \
        \
        while (1) { \
            loop(); \
        } \
    }

typedef struct { uint64_t pid; char name[64]; } ModEntry;

static uint64_t find_mod(const char* name) {
    unsigned long long buf = mod_syscall(SYSCALL_ALLOC_PAGES, 2, 0, 0);
    if (buf == 0 || buf == (unsigned long long)-1) return 0;
    int cnt = mod_list((void*)buf, 64);
    ModEntry* e = (ModEntry*)buf;
    for (int i = 0; i < cnt; i++) {
        int m = 1;
        for (int j = 0; name[j]; j++) { if (e[i].name[j] != name[j]) { m = 0; break; } }
        if (m && e[i].pid) return e[i].pid;
    }
    return 0;
}

#define MOD_PHYS_MAP   1
#define MOD_BOOTINFO   2
#define MOD_BOOTINFO_SIZE 3
#define MOD_PORT_IO    4
#define MOD_PHYS_ADDR  5
#define MOD_REP_INSW   6
#define MOD_REP_OUTSW  7
#define MOD_CHANGE_PROCESS_NAME 8
#define MOD_UNREGISTER 9

#endif // MODSTD_H