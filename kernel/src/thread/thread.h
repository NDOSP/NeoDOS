#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} ThreadState;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) ThreadContext;

typedef struct Process {
    uint64_t pid;
    uint64_t cr3;
} Process;

typedef struct Thread {
    uint64_t tid;
    ThreadState state;
    ThreadContext* context;
    void* stackPtr;
    Process* process;
    struct Thread* next;
} Thread;

// void schedulerInit(void);
// void schedule(void);
// void createThread(Process* process, void (*entryPoint)(void));

#endif // THREAD_H