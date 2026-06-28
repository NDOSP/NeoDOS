#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "interrupts/idt.h"

typedef struct SyscallFrame SyscallFrame;

#define MAX_TASKS 64
#define TIME_SLICE 5

#define IPC_MSG_SIZE 64
#define IPC_MAX_MSG 16

typedef struct {
    uint64_t senderPid;
    uint64_t data[IPC_MSG_SIZE / 8];
} Message;

typedef struct {
    Message msgs[IPC_MAX_MSG];
    volatile uint32_t head;
    volatile uint32_t tail;
} Mailbox;

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
} TaskState;

typedef struct Task {
    struct Task* next;
    struct Task* prev;
    uint64_t id;
    char name[24];
    TaskState state;
    uint64_t ticksLeft;
    uint64_t cr3;
    Mailbox mailbox;
    INTERRUPT_FRAME frame;
} Task;

void schedulerInit(void);
Task* createTask(void (*entry)(void), const char* name);
Task* findTask(uint64_t pid);
uint64_t getCurrentPid(void);
Task* getCurrentTask(void);
uint64_t forkTask(const SyscallFrame* parent);
void exitTask(void);

#endif
