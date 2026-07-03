// Legacy compatibility header — all module APIs are now in modstd.h

#ifndef MODLIB_H
#define MODLIB_H

#include <stdint.h>
#include <stddef.h>
#include "module.h"

// ======================== Syscall numbers ========================
#include "../../libs/syscalls.h"

// ======================== SYSCALL_MOD subfunctions ========================
#define MOD_PHYS_MAP   1
#define MOD_BOOTINFO   2
#define MOD_BOOTINFO_SIZE 3
#define MOD_PORT_IO    4
#define MOD_PHYS_ADDR  5
#define MOD_REP_INSW   6
#define MOD_REP_OUTSW  7

// BOOTINFO types
#define BOOTINFO_FB     1
#define BOOTINFO_FONT   2
#define BOOTINFO_MODCNT 3
#define BOOTINFO_MOD    4
#define BOOTINFO_FONT_SCALE 5

// PORT_IO helpers
#define PORT_IN  0
#define PORT_OUT 1
#define PORT_W(port, width, dir) \
    ((uint64_t)((port) & 0xFFFF) | (((uint64_t)(width) & 0xFF) << 16) | (((uint64_t)(dir) & 0xFF) << 24))

// BOOTINFO arg2 builder: type | (index << 8) | (max_size << 32)
#define BOOTINFO_ARG(type, max_size)   ((uint64_t)(type) | ((uint64_t)(max_size) << 32))
#define BOOTINFO_ARG_IDX(type, idx, max_size) \
    ((uint64_t)(type) | ((uint64_t)(idx) << 8) | ((uint64_t)(max_size) << 32))

// ======================== Syscall wrappers ========================
static inline unsigned long long mod_syscall(unsigned long long n,
                                              unsigned long long a1,
                                              unsigned long long a2,
                                              unsigned long long a3) {
    return syscall(n, a1, a2, a3, 0);
}

static inline unsigned long long mod_syscall4(unsigned long long n,
                                               unsigned long long a1,
                                               unsigned long long a2,
                                               unsigned long long a3,
                                               unsigned long long a4) {
    return syscall(n, a1, a2, a3, a4);
}

// ======================== Debug serial output ========================
static inline void debug_puts(const char* s) {
    unsigned long long len = 0;
    while (s[len]) len++;
    mod_syscall(SYSCALL_WRITE, (unsigned long long)s, len, 0);
}

// ======================== IPC wrappers ========================
static inline unsigned long long mod_send(unsigned long long pid, void* buf) {
    return send(pid, buf);
}

static inline unsigned long long mod_recv(void* buf) {
    return recv(buf);
}

static inline unsigned long long mod_recv_from(unsigned long long pid, void* buf) {
    return mod_syscall(SYSCALL_RECV_FROM, pid, (unsigned long long)buf, 0);
}

// ======================== Module registry ========================
static inline int mod_list(void* entries, int max) {
    return list_mods(entries, max);
}

// ======================== Module-only helpers (SYSCALL_MOD) ========================
static inline unsigned long long mod_phys_addr(unsigned long long vaddr) {
    return mod_syscall4(SYSCALL_MOD, MOD_PHYS_ADDR, vaddr, 0, 0);
}

static inline unsigned long long mod_rep_insw(unsigned long long port,
                                              unsigned long long buf_phys,
                                              unsigned long long words) {
    return mod_syscall4(SYSCALL_MOD, MOD_REP_INSW, port, buf_phys, words);
}

static inline unsigned long long mod_rep_outsw(unsigned long long port,
                                               unsigned long long buf_phys,
                                               unsigned long long words) {
    return mod_syscall4(SYSCALL_MOD, MOD_REP_OUTSW, port, buf_phys, words);
}

// ======================== NDR registry (available to everyone) ========================
static inline unsigned long long ndr_size(void) {
    return mod_syscall(SYSCALL_NDR, NDR_SIZE, 0, 0);
}

static inline unsigned long long ndr_copy(void* dest, unsigned long long offset, unsigned long long size) {
    return mod_syscall4(SYSCALL_NDR, NDR_COPY, (unsigned long long)dest, offset, size);
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
