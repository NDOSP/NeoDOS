#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "interrupts/idt.h"

typedef struct SyscallFrame SyscallFrame;

#define MAX_TASKS 64
#define TIME_SLICE 5
#define PRIORITY_LOW   0
#define PRIORITY_NORM  64
#define PRIORITY_HIGH  128
#define PRIORITY_RTIME 192
#define PRIORITY_MAX   255

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
    uint8_t priority;          // 0=lowest, 255=highest
    int isModule;              // 1 if this task is a module (can use SYSCALL_MOD)
    Mailbox mailbox;
    INTERRUPT_FRAME frame;
    uint64_t vaddr_next;       // next virtual address for auto-allocation (SYSCALL_ALLOC_PAGES)
} Task;

void schedulerInit(void);
Task* createTask(void (*entry)(void), const char* name);
Task* createTaskPrio(void (*entry)(void), const char* name, uint8_t priority);
Task* createUserTask(void (*entry)(void), const char* name);
Task* createUserTaskPrio(void (*entry)(void), const char* name, uint8_t priority);
void addTaskToReadyQueue(Task* task);
Task* findTask(uint64_t pid);
uint64_t getCurrentPid(void);
Task* getCurrentTask(void);
uint64_t forkTask(const SyscallFrame* parent);
void exitTask(void);
int setTaskPriority(uint64_t pid, uint8_t priority);
int getTaskPriority(uint64_t pid, uint8_t* priority);
void launchModules(void);
uint64_t schedulerBlockAndSwitch(SyscallFrame* sf);
int schedulerWake(uint64_t pid);
void killTaskAndSwitch(INTERRUPT_FRAME* frame);
int changeTaskName(uint64_t pid, char name[24]);

#endif
