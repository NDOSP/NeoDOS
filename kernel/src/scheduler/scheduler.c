#include "scheduler.h"
#include "interrupts/lapic.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "debug.h"
#include "string.h"
#include "syscalls/syscalls.h"

extern uint64_t _reg_timerHz;

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
    // timer count = (337 * 0x100000) / timerHz
    lapic_write(0x380, (0x33700000U) / (_reg_timerHz ? _reg_timerHz : 337));
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

uint64_t forkTask(const SyscallFrame* sf) {
    if (taskCount >= MAX_TASKS) return -1;

    uint64_t id = taskCount++;
    Task* child = &taskPool[id];
    child->id = id;
    child->state = TASK_READY;
    child->ticksLeft = TIME_SLICE;
    child->cr3 = 0;

    for (uint32_t i = 0; i < sizeof(child->name) - 1 && currentTask->name[i]; i++)
        child->name[i] = currentTask->name[i];
    child->name[sizeof(child->name) - 1] = '\0';

    void* stack = pmmAllocator(4);
    if (!stack) return -1;
    addPageRange((uint64_t)stack, 4 * PAGE_SIZE, (uint64_t)stack, PAGE_PRESENT | PAGE_WRITE);

    INTERRUPT_FRAME* f = &child->frame;
    memset(f, 0, sizeof(INTERRUPT_FRAME));
    f->cs = 0x08;
    f->ss = 0x10;
    f->rflags = sf->rflags | 0x200;
    f->rip = sf->rip;
    f->rsp = (uint64_t)stack + 4 * PAGE_SIZE;
    f->rax = 0;
    f->r15 = sf->r15;
    f->r14 = sf->r14;
    f->r13 = sf->r13;
    f->r12 = sf->r12;
    f->r10 = sf->r10;
    f->r9  = sf->r9;
    f->r8  = sf->r8;
    f->rdi = sf->rdi;
    f->rsi = sf->rsi;
    f->rbp = sf->rbp;
    f->rbx = sf->rbx;
    f->rdx = sf->rdx;

    if (!readyHead) {
        readyHead = child;
        child->next = child;
        child->prev = child;
    } else {
        Task* last = readyHead->prev;
        child->next = readyHead;
        child->prev = last;
        last->next = child;
        readyHead->prev = child;
    }

    DEBUG_INFO("SCHED: fork -> child pid=%lu", id);
    return id;
}

void exitTask(void) {
    DEBUG_INFO("SCHED: task '%s' (pid=%lu) exiting", currentTask->name, currentTask->id);
    currentTask->state = TASK_DEAD;
    currentTask->prev->next = currentTask->next;
    currentTask->next->prev = currentTask->prev;

    while (1) {
        asm volatile("hlt");
    }
}

Task* findTask(uint64_t pid) {
    if (pid == 0) return currentTask;
    if (pid >= MAX_TASKS) return NULL;
    Task* t = &taskPool[pid];
    if (t->state == TASK_DEAD) return NULL;
    return t;
}

uint64_t getCurrentPid(void) {
    return currentTask ? currentTask->id : 0;
}

Task* getCurrentTask(void) {
    return currentTask;
}
