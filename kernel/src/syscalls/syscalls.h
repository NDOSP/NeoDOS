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

#define SYSCALL_EXIT   1
#define SYSCALL_FORK   2
#define SYSCALL_EXEC   3
#define SYSCALL_GETPID 4
#define SYSCALL_SEND   5
#define SYSCALL_RECV   6

typedef struct SyscallFrame {
    uint64_t r15, r14, r13, r12, r11b, r10, r9, r8;
    uint64_t rcx2, rdx, rsi, rdi;
    uint64_t rbx, rbp;
    uint64_t rip, rflags, userRsp;
} __attribute__((packed)) SyscallFrame;

void initSyscalls(uint64_t cpuId, void* kStack);

#endif // SYSCALLS_H