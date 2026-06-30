#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include "percpu.h"

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084
#define MSR_GS_BASE 0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

// Basic syscalls
#define SYSCALL_EXIT        1
#define SYSCALL_FORK        2
#define SYSCALL_GETPID      3
#define SYSCALL_SEND        4
#define SYSCALL_RECV        5
// Memory management
#define SYSCALL_ALLOC_PAGES 6
#define SYSCALL_FREE_PAGES  7
// Module management
#define SYSCALL_MOD_REGISTER   10
#define SYSCALL_MOD_UNREGISTER 11
#define SYSCALL_MOD_LIST       12
// Module-only syscall (requires task->isModule == 1)
//   sub=1: PHYS_MAP  (arg2=paddr, arg3=pages       -> vaddr)
//   sub=2: BOOTINFO  (arg2=type|size<<32, arg3=buf  -> bytes)
//   sub=3: BOOTINFO_SIZE (arg2=type                 -> size)
//   sub=4: PORT_IO   (arg2=port|w<<16|d<<24, arg3=val -> in val / 0)
#define SYSCALL_MOD            20
// Debug
#define SYSCALL_WRITE       0xFF00000000000001

typedef struct SyscallFrame {
    uint64_t r15, r14, r13, r12, r11b, r10, r9, r8;
    uint64_t rcx2, rdx, rsi, rdi;
    uint64_t rbx, rbp;
    uint64_t rip, rflags, userRsp;
} __attribute__((packed)) SyscallFrame;

void initSyscalls(uint64_t cpuId, void* kStack);

#endif // SYSCALLS_H