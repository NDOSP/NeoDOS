#include "syscalls.h"
#include "scheduler/scheduler.h"
#include "ipc/ipc.h"
#include "memory/memutils.h"
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
    (void)arg1;
    (void)arg2;
    (void)arg3;

    switch (num) {
    case SYSCALL_EXIT:
        exitTask();
        return 0;

    case SYSCALL_FORK:
        return forkTask(sf);

    case SYSCALL_SEND: {
        uint64_t buf[IPC_MSG_SIZE / 8];
        memcpy((void*)buf, (void*)arg2, IPC_MSG_SIZE);
        return ipcSendPid(arg1, getCurrentPid(), buf);
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

    // TODO: Remove SYSCALL_WRITE
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