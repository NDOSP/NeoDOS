#ifndef MODLIB_H
#define MODLIB_H

#include <stdint.h>
#include <stddef.h>
#include "module.h"

// ======================== Syscall numbers ========================
#define SYSCALL_EXIT        1
#define SYSCALL_FORK        2
#define SYSCALL_GETPID      3
#define SYSCALL_SEND        4
#define SYSCALL_RECV        5
#define SYSCALL_ALLOC_PAGES 6
#define SYSCALL_FREE_PAGES  7
#define SYSCALL_MOD_REGISTER   10
#define SYSCALL_MOD_UNREGISTER 11
#define SYSCALL_MOD_LIST       12
#define SYSCALL_MOD            20
#define SYSCALL_WRITE       0xFF00000000000001

// ======================== SYSCALL_MOD subfunctions ========================
#define MOD_PHYS_MAP   1
#define MOD_BOOTINFO   2
#define MOD_BOOTINFO_SIZE 3
#define MOD_PORT_IO    4

// BOOTINFO types
#define BOOTINFO_FB     1
#define BOOTINFO_FONT   2
#define BOOTINFO_MODCNT 3
#define BOOTINFO_MOD    4

// PORT_IO helpers
#define PORT_IN  0
#define PORT_OUT 1
#define PORT_W(port, width, dir) \
    ((uint64_t)((port) & 0xFFFF) | (((uint64_t)(width) & 0xFF) << 16) | (((uint64_t)(dir) & 0xFF) << 24))

// BOOTINFO arg2 builder: type | (index << 8) | (max_size << 32)
#define BOOTINFO_ARG(type, max_size)   ((uint64_t)(type) | ((uint64_t)(max_size) << 32))
#define BOOTINFO_ARG_IDX(type, idx, max_size) \
    ((uint64_t)(type) | ((uint64_t)(idx) << 8) | ((uint64_t)(max_size) << 32))

// ======================== Basic syscall wrapper ========================
static inline unsigned long long mod_syscall(unsigned long long n,
                                              unsigned long long a1,
                                              unsigned long long a2,
                                              unsigned long long a3) {
    unsigned long long ret;
    register unsigned long long rax asm("rax") = n;
    register unsigned long long rdi asm("rdi") = a1;
    register unsigned long long rsi asm("rsi") = a2;
    register unsigned long long rdx asm("rdx") = a3;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx)
                 : "rcx", "r11", "memory");
    return ret;
}

// ======================== Debug serial output ========================
static inline void debug_puts(const char* s) {
    unsigned long long len = 0;
    while (s[len]) len++;
    mod_syscall(SYSCALL_WRITE, (unsigned long long)s, len, 0);
}

// ======================== IPC wrappers ========================
static inline unsigned long long mod_send(unsigned long long pid, void* buf) {
    return mod_syscall(SYSCALL_SEND, pid, (unsigned long long)buf, 0);
}

static inline unsigned long long mod_recv(void* buf) {
    return mod_syscall(SYSCALL_RECV, (unsigned long long)buf, 0, 0);
}

// ======================== Module registry ========================
static inline int mod_register(const char* name) {
    return (int)mod_syscall(SYSCALL_MOD_REGISTER, (unsigned long long)name, 0, 0);
}

static inline int mod_unregister(void) {
    return (int)mod_syscall(SYSCALL_MOD_UNREGISTER, 0, 0, 0);
}

static inline int mod_list(void* entries, int max) {
    return (int)mod_syscall(SYSCALL_MOD_LIST, (unsigned long long)entries,
                            (unsigned long long)max, 0);
}

// ======================== Number formatting (itoa) ========================
// Writes decimal representation of val into buf (max chars).
// Returns number of chars written (not including null terminator).
static inline int mod_itoa(unsigned long long val, char* buf, int max) {
    if (max <= 0) return 0;
    int idx = 0;
    // Handle 0 explicitly
    if (val == 0 && idx < max - 1) {
        buf[idx++] = '0';
    } else {
        char tmp[24];
        int ti = 0;
        while (val > 0 && ti < 24) {
            tmp[ti++] = '0' + (val % 10);
            val /= 10;
        }
        for (int i = ti - 1; i >= 0 && idx < max - 1; i--)
            buf[idx++] = tmp[i];
    }
    buf[idx] = '\0';
    return idx;
}

static inline int mod_itoa_s(int val, char* buf, int max) {
    if (max <= 0) return 0;
    int idx = 0;
    if (val < 0 && idx < max - 1) {
        buf[idx++] = '-';
        val = -val;
    }
    return idx + mod_itoa((unsigned long long)(unsigned int)val, buf + idx, max - idx);
}

// Prints a uint64_t to serial
static inline void debug_putu(unsigned long long val) {
    char buf[24];
    mod_itoa(val, buf, 24);
    debug_puts(buf);
}

#endif
