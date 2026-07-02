#include "scheduler.h"
#include "interrupts/lapic.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "debug.h"
#include "string.h"
#include "syscalls/syscalls.h"
#include "bootinfo.h"
#include "modman/modman.h"

// Default timer frequency for LAPIC timer
#define DEFAULT_TIMER_HZ 337

extern void idt32Stub(void);
extern void registerInterruptHandler(uint8_t n, void (*h)(INTERRUPT_FRAME*));
extern void idtSetEntry(uint8_t num, void (*handler)(void), uint8_t type_attr, uint8_t ist);

static Task taskPool[MAX_TASKS];
static Task* currentTask = NULL;
static Task* readyHead = NULL;
static uint64_t taskCount = 0;
static uint64_t totalTicks = 0;
static uint64_t kernelCr3 = 0;

static void idleTask(void) {
    while (1) {
        asm volatile("hlt");
    }
}

static Task* pickNext(void) {
    if (!readyHead) return NULL;
    Task* best = NULL;
    uint8_t bestPrio = 0;
    Task* start = currentTask ? currentTask : readyHead;
    Task* t = start;
    int iter = 0;
    do {
        if (t->state == TASK_READY && t->priority >= bestPrio) {
            if (t->priority > bestPrio || !best) {
                best = t;
                bestPrio = t->priority;
            }
        }
        t = t->next;
        if (++iter > MAX_TASKS) break;
    } while (t != start);

    if (!best) return NULL;

    // Within same priority, round-robin: pick the next READY task at or after current
    if (bestPrio == (currentTask ? currentTask->priority : 0)) {
        Task* rr = start->next;
        iter = 0;
        while (rr != start) {
            if (rr->state == TASK_READY && rr->priority == bestPrio) return rr;
            rr = rr->next;
            if (++iter > MAX_TASKS) break;
        }
    }

    return best;
}

static inline void fixGsBeforeReturn(uint64_t oldCs, INTERRUPT_FRAME* frame) {
    // If we entered the ISR from kernel mode (GS = kernel GS base via swapgs
    // in the syscall entry) and we're returning to a user task, swapgs back
    // to user GS (= 0) so the task's first syscall works correctly.
    if (oldCs == 0x10 && frame->cs == 0x2B)
        asm volatile("swapgs");
}

void timerHandler(INTERRUPT_FRAME* frame) {
    lapic_write(0xB0, 0);

    totalTicks++;

    // If current task is dead, don't re-queue it — pick next directly.
    if (currentTask->state == TASK_DEAD) {
        Task* next = pickNext();
        if (!next) return;
        uint64_t oldCs = frame->cs;
        next->state = TASK_RUNNING;
        *frame = next->frame;
        currentTask = next;
        fixGsBeforeReturn(oldCs, frame);
        asm volatile("mov %0, %%cr3" : : "r"(next->cr3 ? next->cr3 : kernelCr3) : "memory");
        return;
    }

    if (--currentTask->ticksLeft > 0) return;
    currentTask->ticksLeft = TIME_SLICE;

    uint64_t oldCs = frame->cs;

    if (currentTask->state != TASK_BLOCKED) {
        currentTask->state = TASK_READY;
        currentTask->frame = *frame;
    } else {
        uint64_t savedRax = currentTask->frame.rax;
        currentTask->frame = *frame;
        currentTask->frame.rax = savedRax;
    }

    Task* next = pickNext();
    if (!next || next == currentTask) return;

    next->state = TASK_RUNNING;
    *frame = next->frame;

    currentTask = next;

    fixGsBeforeReturn(oldCs, frame);

    uint64_t target_cr3 = next->cr3 ? next->cr3 : kernelCr3;
    asm volatile("mov %0, %%cr3" : : "r"(target_cr3) : "memory");
}

void schedulerInit(void) {
    asm volatile("mov %%cr3, %0" : "=r"(kernelCr3));

    idtSetEntry(32, idt32Stub, 0x8E, 0);
    registerInterruptHandler(32, timerHandler);

    currentTask = createTask(idleTask, "idle");
    currentTask->state = TASK_RUNNING;

    lapic_write(0x3E0, 0x0B);
    lapic_write(0x320, 32 | (1 << 17) | (1 << 16));
    // timer count = (337 * 0x100000) / timerHz
    lapic_write(0x380, (0x33700000U) / DEFAULT_TIMER_HZ);
    lapic_write(0x320, 32 | (1 << 17));

    DEBUG_INFO("SCHED: initialized");
}

void addTaskToReadyQueue(Task* task) {
    // Add task to ready queue
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
}

Task* createTask(void (*entry)(void), const char* name) {
    return createTaskPrio(entry, name, PRIORITY_NORM);
}

Task* createTaskPrio(void (*entry)(void), const char* name, uint8_t priority) {
    if (taskCount >= MAX_TASKS) return NULL;

    uint64_t id = taskCount++;
    Task* task = &taskPool[id];
    task->id = id;
    task->state = TASK_READY;
    task->ticksLeft = TIME_SLICE;
    task->cr3 = 0;
    task->priority = priority;
    task->isModule = 0;

    uint32_t i;
    for (i = 0; i < sizeof(task->name) - 1 && name[i]; i++)
        task->name[i] = name[i];
    task->name[i] = '\0';

    void* stack = pmmAllocator(4);
    if (!stack) return NULL;
    addPageRange((uint64_t)stack, 4 * PAGE_SIZE, (uint64_t)stack, PAGE_PRESENT | PAGE_WRITE);

    memset(&task->frame, 0, sizeof(INTERRUPT_FRAME));
    task->frame.cs = 0x10;
    task->frame.ss = 0x18;
    task->frame.rflags = 0x202;
    task->frame.rip = (uint64_t)entry;
    task->frame.rsp = (uint64_t)stack + 4 * PAGE_SIZE;

    addTaskToReadyQueue(task);

    DEBUG_INFO("SCHED: task '%s' created (id=%lu)", name, id);
    return task;
}

Task* createUserTask(void (*entry)(void), const char* name) {
    return createUserTaskPrio(entry, name, PRIORITY_NORM);
}

Task* createUserTaskPrio(void (*entry)(void), const char* name, uint8_t priority) {
    if (taskCount >= MAX_TASKS) return NULL;

    uint64_t id = taskCount++;
    Task* task = &taskPool[id];
    task->id = id;
    task->state = TASK_READY;
    task->ticksLeft = TIME_SLICE;
    task->cr3 = vmm_create_user_pml4();
    task->priority = priority;
    task->isModule = 0;
    task->vaddr_next = 0x1000000;

    uint32_t i;
    for (i = 0; i < sizeof(task->name) - 1 && name[i]; i++)
        task->name[i] = name[i];
    task->name[i] = '\0';

    // User task stack will be provided by the ELF loader, so we don't allocate here
    // We just initialize the frame with user-mode selectors
    memset(&task->frame, 0, sizeof(INTERRUPT_FRAME));
    task->frame.cs = 0x2B;  // User code selector (index 5, RPL=3)
    task->frame.ss = 0x23;  // User data selector (index 4, RPL=3)
    task->frame.rflags = 0x202;
    task->frame.rip = (uint64_t)entry;
    task->frame.rsp = 0;  // Will be set by caller

    // DO NOT add to ready queue yet - caller needs to set rip/rsp first
    task->next = NULL;
    task->prev = NULL;

    DEBUG_INFO("SCHED: user task '%s' (prio=%u) created (id=%lu)", name, priority, id);
    return task;
}



uint64_t forkTask(const SyscallFrame* sf) {
    if (taskCount >= MAX_TASKS) return -1;

    uint64_t id = taskCount++;
    Task* child = &taskPool[id];
    child->id = id;
    child->state = TASK_READY;
    child->ticksLeft = TIME_SLICE;
    child->cr3 = vmm_create_user_pml4();
    child->priority = currentTask->priority;
    child->isModule = currentTask->isModule;
    child->vaddr_next = currentTask->vaddr_next;

    for (uint32_t i = 0; i < sizeof(child->name) - 1 && currentTask->name[i]; i++)
        child->name[i] = currentTask->name[i];
    child->name[sizeof(child->name) - 1] = '\0';

    uint64_t parentCs = currentTask->frame.cs;
    int isUser = (parentCs == 0x2B || currentTask->frame.ss == 0x23);

    void* stack = pmmAllocator(4);
    if (!stack) return -1;
    uint64_t stackFlags = PAGE_PRESENT | PAGE_WRITE;
    if (isUser) stackFlags |= PAGE_USER;
    vmm_map_in_cr3(child->cr3, (uint64_t)stack, 4 * PAGE_SIZE, (uint64_t)stack, stackFlags);

    INTERRUPT_FRAME* f = &child->frame;
    memset(f, 0, sizeof(INTERRUPT_FRAME));
    f->cs = isUser ? 0x2B : 0x10;
    f->ss = isUser ? 0x23 : 0x18;
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
    Task* dead = currentTask;
    dead->state = TASK_DEAD;
    dead->prev->next = dead->next;
    dead->next->prev = dead->prev;

    if (readyHead == dead)
        readyHead = (dead->next != dead) ? dead->next : NULL;

    // TODO: Switch context
}

void killTaskAndSwitch(INTERRUPT_FRAME* frame) {
    Task* dead = currentTask;
    DEBUG_INFO("SCHED: killing task '%s' (pid=%lu)", dead->name, dead->id);

    dead->state = TASK_DEAD;
    dead->prev->next = dead->next;
    dead->next->prev = dead->prev;

    if (readyHead == dead)
        readyHead = (dead->next != dead) ? dead->next : NULL;

    Task* next = pickNext();
    if (!next) {
        serial_printf("SCHED: FATAL - no tasks left after killing '%s' (pid=%lu), halting\n", dead->name, dead->id);
        asm volatile("hlt");
        while (1);
    }

    currentTask = next;
    next->state = TASK_RUNNING;
    *frame = next->frame;

    uint64_t target_cr3 = next->cr3 ? next->cr3 : kernelCr3;
    asm volatile("mov %0, %%cr3" : : "r"(target_cr3) : "memory");
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

int setTaskPriority(uint64_t pid, uint8_t priority) {
    Task* t = findTask(pid);
    if (!t) return -1;
    t->priority = priority;
    return 0;
}

int getTaskPriority(uint64_t pid, uint8_t* priority) {
    Task* t = findTask(pid);
    if (!t || !priority) return -1;
    *priority = t->priority;
    return 0;
}

int changeTaskName(uint64_t pid, char name[24]) {
    Task* t = findTask(pid);
    if (!t || !name) return -1;
    memcpy(t->name, name, 24);
    return 0;
}

static void syscallFrameToInterrupt(const SyscallFrame* sf, INTERRUPT_FRAME* iframe) {
    iframe->r15 = sf->r15;
    iframe->r14 = sf->r14;
    iframe->r13 = sf->r13;
    iframe->r12 = sf->r12;
    iframe->r11 = sf->r11b;
    iframe->r10 = sf->r10;
    iframe->r9  = sf->r9;
    iframe->r8  = sf->r8;
    iframe->rcx = sf->rcx2;
    iframe->rdx = sf->rdx;
    iframe->rsi = sf->rsi;
    iframe->rdi = sf->rdi;
    iframe->rbx = sf->rbx;
    iframe->rbp = sf->rbp;
    iframe->rip = sf->rip;
    iframe->rflags = sf->rflags;
    iframe->rsp = sf->userRsp;
    iframe->cs = 0x2B;
    iframe->ss = 0x23;
}

static void interruptFrameToSyscallStack(const INTERRUPT_FRAME* iframe, SyscallFrame* sf) {
    sf->r15 = iframe->r15;
    sf->r14 = iframe->r14;
    sf->r13 = iframe->r13;
    sf->r12 = iframe->r12;
    sf->r11b = iframe->r11;
    sf->r10 = iframe->r10;
    sf->r9  = iframe->r9;
    sf->r8  = iframe->r8;
    sf->rcx2 = iframe->rcx;
    sf->rdx = iframe->rdx;
    sf->rsi = iframe->rsi;
    sf->rdi = iframe->rdi;
    sf->rbx = iframe->rbx;
    sf->rbp = iframe->rbp;
    sf->rip = iframe->rip;
    sf->rflags = iframe->rflags;
    sf->userRsp = iframe->rsp;
}

static Task* pickReadyUserTask(void) {
    if (!readyHead) return NULL;
    Task* start = currentTask ? currentTask : readyHead;
    Task* t = start;
    int iter = 0;
    do {
        if (t->state == TASK_READY && t->cr3)
            return t;
        t = t->next;
        if (++iter > MAX_TASKS) break;
    } while (t != start);
    return NULL;
}

uint64_t schedulerBlockAndSwitch(SyscallFrame* sf) {
    Task* self = currentTask;
    syscallFrameToInterrupt(sf, &self->frame);
    self->frame.rax = -1;
    self->state = TASK_BLOCKED;

    Task* next = pickReadyUserTask();
    if (!next || next == self) {
        self->state = TASK_READY;
        return -1;
    }

    uint64_t next_rax = next->frame.rax;
    currentTask = next;
    next->state = TASK_RUNNING;
    interruptFrameToSyscallStack(&next->frame, sf);

    asm volatile("mov %0, %%cr3" : : "r"(next->cr3) : "memory");

    return next_rax;
}

int schedulerWake(uint64_t pid) {
    Task* t = findTask(pid);
    if (!t) return -1;
    if (t->state == TASK_BLOCKED) {
        t->state = TASK_READY;
        return 0;
    }
    return -1;
}

void launchModules(void) {
    for (uint64_t i = 0; i < bInfo.moduleCount; i++) {
        void* entry = bInfo.modules[i].data;
        uint64_t modSize = bInfo.modules[i].size;
        if (!entry) continue;

        // Parse .modinfo header to get entry offset (pid=0 = parse only, no register)
        uint64_t entry_off = 0;
        int has_modinfo = (modman_register_embedded(0, entry, modSize, &entry_off) == 0);

        // Allocate user stack
        size_t stack_pages = 4;
        void* stack_phys = pmmAllocator(stack_pages);
        if (!stack_phys) {
            DEBUG_WARN("SCHED: failed to allocate stack for module '%s'", bInfo.modules[i].name);
            continue;
        }

        Task* task = createUserTaskPrio((void (*)(void))((uint64_t)entry + entry_off),
                                        bInfo.modules[i].name, PRIORITY_NORM);
        if (task) {
            task->isModule = 1;

            // Map module code/data pages into the task's page table
            uint64_t modPages = (modSize + PAGE_SIZE - 1) / PAGE_SIZE;
            vmm_map_in_cr3(task->cr3, (uint64_t)entry, modPages * PAGE_SIZE,
                           (uint64_t)entry, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

            vmm_map_in_cr3(task->cr3, (uint64_t)stack_phys, stack_pages * PAGE_SIZE,
                           (uint64_t)stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            task->frame.rsp = (uint64_t)stack_phys + stack_pages * PAGE_SIZE;

            // Register with real PID
            if (has_modinfo)
                modman_register_embedded(task->id, entry, modSize, NULL);

            addTaskToReadyQueue(task);
            DEBUG_INFO("SCHED: module '%s' launched (entry=%lX, size=%lu, pages=%lu)",
                       bInfo.modules[i].name, (uint64_t)entry, modSize, modPages);
        }
    }
}
