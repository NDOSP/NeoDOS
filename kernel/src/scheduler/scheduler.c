#include "scheduler.h"
#include "interrupts/lapic.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "debug.h"
#include "string.h"

extern void idt32Stub(void);
extern void registerInterruptHandler(uint8_t n, void (*h)(INTERRUPT_FRAME*));
extern void idtSetEntry(uint8_t num, void (*handler)(void), uint8_t type_attr, uint8_t ist);

static Task taskPool[MAX_TASKS];
static Task* currentTask = NULL;
static Task* readyHead = NULL;
static uint64_t taskCount = 0;
static uint64_t totalTicks = 0;

static void idleTask(void) {
    while (1) {
        asm volatile("hlt");
    }
}

static Task* pickNext(void) {
    if (!readyHead) return NULL;
    Task* start = currentTask ? currentTask : readyHead;
    Task* t = start->next;

    while (t != start) {
        if (t->state == TASK_READY) return t;
        t = t->next;
    }

    if (start->state == TASK_READY) return start;
    return NULL;
}

void timerHandler(INTERRUPT_FRAME* frame) {
    lapic_write(0xB0, 0);

    totalTicks++;

    if (--currentTask->ticksLeft > 0) return;
    currentTask->ticksLeft = TIME_SLICE;

    Task* next = pickNext();
    if (!next || next == currentTask) return;

    currentTask->state = TASK_READY;
    currentTask->frame = *frame;

    next->state = TASK_RUNNING;
    *frame = next->frame;

    currentTask = next;
}

void schedulerInit(void) {
    idtSetEntry(32, idt32Stub, 0x8E, 2);
    registerInterruptHandler(32, timerHandler);

    currentTask = createTask(idleTask, "idle");
    currentTask->state = TASK_RUNNING;

    lapic_write(0x3E0, 0x0B);
    lapic_write(0x320, 32 | (1 << 17) | (1 << 16));
    lapic_write(0x380, 0x100000);
    lapic_write(0x320, 32 | (1 << 17));

    DEBUG_INFO("SCHED: initialized");
}

Task* createTask(void (*entry)(void), const char* name) {
    if (taskCount >= MAX_TASKS) return NULL;

    uint64_t id = taskCount++;
    Task* task = &taskPool[id];
    task->id = id;
    task->state = TASK_READY;
    task->ticksLeft = TIME_SLICE;
    task->cr3 = 0;

    uint32_t i;
    for (i = 0; i < sizeof(task->name) - 1 && name[i]; i++)
        task->name[i] = name[i];
    task->name[i] = '\0';

    void* stack = pmmAllocator(4);
    if (!stack) return NULL;
    addPageRange((uint64_t)stack, 4 * PAGE_SIZE, (uint64_t)stack, PAGE_PRESENT | PAGE_WRITE);

    memset(&task->frame, 0, sizeof(INTERRUPT_FRAME));
    task->frame.cs = 0x08;
    task->frame.ss = 0x10;
    task->frame.rflags = 0x202;
    task->frame.rip = (uint64_t)entry;
    task->frame.rsp = (uint64_t)stack + 4 * PAGE_SIZE;

    if (!readyHead) {
        readyHead = task;
        task->next = task;
        task->prev = task;
    } else {
        Task* last = readyHead->prev;
        task->next = readyHead;
        task->prev = last;
        last->next = task;
        readyHead->prev = task;
    }

    DEBUG_INFO("SCHED: task '%s' created (id=%lu)", name, id);
    return task;
}
