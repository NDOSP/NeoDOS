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
    static ModTable __attribute__((section(".modtable.table"))) __modtable; \
    static int __modtable_index = 0; \
    \
    void init(void); \
    void loop(void); \
    \
    __attribute__((section(".text.start"))) \
    void _start(void) { \
        __publish_modtable(); \
        __modtable.pid = get_pid(); \
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

#define MODTABLE_FUNCTION __attribute__((section(".modtable.function"))) 
#define MY_PID __modtable.pid

extern char __modtable_end[];
extern char __modtable_start[];

static uint64_t __modtable_shm = (uint64_t)-1;

static void __publish_modtable() {
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
}

#define REGISTER_FUNCTION(name) \
    __modtable.functions[__modtable_index++] = name - (uint64_t)__modtable_start;

static int send_mod_table(uint64_t pid) {
    uint64_t msg[8] = {0};
    msg[0] = __modtable_shm;
    send(pid, msg);
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

#define BOOTINFO_FB     1
#define BOOTINFO_FONT   2
#define BOOTINFO_MODCNT 3
#define BOOTINFO_MOD    4
#define BOOTINFO_FONT_SCALE 5

#define BOOTINFO_ARG(type, max_size)   ((uint64_t)(type) | ((uint64_t)(max_size) << 32))
#define BOOTINFO_ARG_IDX(type, idx, max_size) \
    ((uint64_t)(type) | ((uint64_t)(idx) << 8) | ((uint64_t)(max_size) << 32))

#define PORT_IN  0
#define PORT_OUT 1
#define PORT_W(port, width, dir) \
    ((uint64_t)((port) & 0xFFFF) | (((uint64_t)(width) & 0xFF) << 16) | (((uint64_t)(dir) & 0xFF) << 24))

#endif // MODSTD_H