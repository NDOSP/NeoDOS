#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "interrupts/idt.h"

#define MAX_TASKS 64
#define TIME_SLICE 5

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
    INTERRUPT_FRAME frame;
} Task;

void schedulerInit(void);
Task* createTask(void (*entry)(void), const char* name);

#endif
