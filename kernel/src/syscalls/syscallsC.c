#include "syscalls.h"
#include "scheduler/scheduler.h"
#include "ipc/ipc.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "serial.h"

extern void syscall_entry(void); 
PerCpuData perCpuArray[255];

void initSyscalls(uint64_t cpuId, void* kStack) {
    perCpuArray[cpuId].cpuId = cpuId;
    perCpuArray[cpuId].kernelStack = kStack;

    uint64_t addr = (uint64_t)&perCpuArray[cpuId];
    asm volatile("wrmsr" : : "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)), "c"(MSR_KERNEL_GS_BASE));

    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_EFER));
    low |= (1 << 11) | 1;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(MSR_EFER));

    uint64_t star = ((uint64_t)0x10 << 32) | ((uint64_t)0x18 << 48);
    asm volatile("wrmsr" : : "a"((uint32_t)star), "d"((uint32_t)(star >> 32)), "c"(MSR_STAR));

    uint64_t lstar = (uint64_t)syscall_entry;
    asm volatile("wrmsr" : : "a"((uint32_t)lstar), "d"((uint32_t)(lstar >> 32)), "c"(MSR_LSTAR));

    asm volatile("wrmsr" : : "a"(0x200), "d"(0), "c"(MSR_SFMASK));
}

uint64_t syscallDispatcher(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, SyscallFrame* sf) {
    (void)sf;

    switch (num) {
    case SYSCALL_EXIT:
        exitTask();
        return 0;

    case SYSCALL_FORK: {
        uint64_t child = forkTask(sf);
        return child;
    }

    case SYSCALL_SEND: {
        uint64_t buf[IPC_MSG_SIZE / 8];
        memcpy((void*)buf, (void*)arg2, IPC_MSG_SIZE);
        uint64_t sender = getCurrentPid();
        return ipcSendPid(arg1, sender, buf);
    }

    case SYSCALL_RECV: {
        uint64_t senderPid;
        uint64_t buf[IPC_MSG_SIZE / 8];
        int ret = ipcRecv(buf, &senderPid);
        if (ret == 0) {
            memcpy((void*)arg1, (void*)buf, IPC_MSG_SIZE);
            return senderPid;
        }
        return -1;
    }

    case SYSCALL_GETPID:
        return getCurrentPid();

    case SYSCALL_ALLOC_PAGES: {
        uint64_t n = arg1;
        if (n == 0 || n > 256) return -1;
        void* phys = pmmAllocator(n);
        if (!phys) return -1;
        addPageRange((uint64_t)phys, n * 4096, (uint64_t)phys,
                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        memset(phys, 0, n * 4096);
        return (uint64_t)phys;
    }

    case SYSCALL_FREE_PAGES: {
        uint64_t phys = arg1;
        uint64_t n = arg2;
        if (n == 0 || n > 256) return -1;
        freePages((void*)phys, n);
        pmmFree((void*)phys, n);
        return 0;
    }

    case SYSCALL_WRITE: {
        char buf[256];
        uint64_t len = arg2 > 255 ? 255 : arg2;
        memcpy(buf, (void*)arg1, len);
        buf[len] = '\0';
        serial_puts(buf);
        return len;
    }

    default:
        return -1;
    }
}