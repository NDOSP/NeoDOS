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

void initSyscalls(uint64_t cpuId, void* kStack);

#endif // SYSCALLS_H