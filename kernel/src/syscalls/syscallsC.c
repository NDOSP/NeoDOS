#include "syscalls.h"
#include "video.h"

extern void syscall_entry(void); 
PerCpuData perCpuArray[255];

void initSyscalls(uint64_t cpuId, void* kStack) {
    perCpuArray[cpuId].cpuId = cpuId;
    perCpuArray[cpuId].kernelStack = kStack;

    uint64_t addr = (uint64_t)&perCpuArray[cpuId];
    asm volatile("wrmsr" : : "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)), "c"(MSR_KERNEL_GS_BASE));

    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_EFER));
    low |= 1;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(MSR_EFER));

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x20 << 48);
    asm volatile("wrmsr" : : "a"((uint32_t)star), "d"((uint32_t)(star >> 32)), "c"(MSR_STAR));

    uint64_t lstar = (uint64_t)syscall_entry;
    asm volatile("wrmsr" : : "a"((uint32_t)lstar), "d"((uint32_t)(lstar >> 32)), "c"(MSR_LSTAR));

    asm volatile("wrmsr" : : "a"(0x200), "d"(0), "c"(MSR_SFMASK));
}

void syscallDispatcher(uint64_t num, uint64_t arg1) {
    if (num == 1) {
        drawOutput("Hello, user space World!\n", white);
    }
}