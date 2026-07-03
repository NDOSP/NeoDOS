#ifndef MODSTD_H
#define MODSTD_H

#include "module.h"
#include "modlib.h"
#include "std.h"

#include <stdint.h>

#define PAGE_SIZE 4096

#define REGISTER_MODULE(name) \
    MODINFO(name); \
    \
    extern void init(void); \
    extern void loop(void); \
    \
    __attribute__((section(".text.start"))) \
    void _start(void) { \
        debug_puts(name); \
        debug_puts(": init | modstd (v1.2)"); \
        debug_puts("\n"); \
        \
        init(); \
        \
        while (1) { \
            loop(); \
        } \
    }

extern char __modtable_end[];
extern char __modtable_start[];

static uint64_t __modtable_shm = (uint64_t)-1;

static uint64_t set_mod_table() {
    if (__modtable_shm != (uint64_t)-1) return __modtable_shm;

    void *start, *end;
    asm volatile(
        "lea __modtable_start(%%rip), %0\n"
        "lea __modtable_end(%%rip), %1"
        : "=r"(start), "=r"(end)
    );

    uint64_t modtable_bytes = (uint64_t)(end - start);

    __modtable_shm = shm_create(PAGES(modtable_bytes));
    if (__modtable_shm == (uint64_t)-1) return (uint64_t)-1;
    unsigned long long shm_vaddr = shm_attach(__modtable_shm);

    memcpy((void*)shm_vaddr, start, modtable_bytes);
    return __modtable_shm;
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