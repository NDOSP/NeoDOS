#include "bootinfo.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "interrupts/idt.h"
#include "tss.h"
#include "acpi.h"
#include "panic.h"
#include "syscalls/syscalls.h"
#include "smp.h"
#include "serial.h"
#include "debug.h"
#include "scheduler/scheduler.h"
#include "interrupts/lapic.h"
#include "registry/reg.h"

extern void loadGdt(uint64_t);

extern char userTaskACodeStart[];
extern char userTaskACodeEnd[];
extern char userTaskBCodeStart[];
extern char userTaskBCodeEnd[];

__attribute__((section(".bootinfo"))) BootInfo bInfo;

uint64_t _reg_timerHz = 337;

static Task* createUserModeTask(void* code_start, void* code_end, const char* name) {
    size_t code_size = (uint8_t*)code_end - (uint8_t*)code_start;
    size_t code_pages = PAGE_ALIGN_UP(code_size) / PAGE_SIZE;
    if (code_pages == 0) code_pages = 1;

    void* code_phys = pmmAllocator(code_pages);
    if (!code_phys) return NULL;

    addPageRange((uint64_t)code_phys, code_pages * PAGE_SIZE, (uint64_t)code_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    memcpy(code_phys, code_start, code_size);

    size_t stack_pages = 4;
    void* stack_phys = pmmAllocator(stack_pages);
    if (!stack_phys) return NULL;
    addPageRange((uint64_t)stack_phys, stack_pages * PAGE_SIZE, (uint64_t)stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    Task* task = createUserTask((void (*)(void))code_phys, name);
    if (!task) return NULL;
    task->frame.rsp = (uint64_t)stack_phys + stack_pages * PAGE_SIZE;
    addTaskToReadyQueue(task);

    DEBUG_INFO("USER: task '%s' code=%lu bytes @ %lX stack=%lX", name, code_size, (uint64_t)code_phys, task->frame.rsp);
    return task;
}

void kmain() {
    serial_init();

    regInit(bInfo.registry.data, bInfo.registry.size);
    _reg_timerHz = regReadU64("SystemConfig/TimerHz", 337);

    initTss();
    loadGdt(0);
    DEBUG_INFO("Hello, NeoDOS booting");

    DEBUG_INFO("initializing IDT");
    idtInit();
    DEBUG_INFO("initializing ACPI");
    acpiInit();
    DEBUG_INFO("initializing syscalls");
    initSyscalls(0, (void*)tss[0].rsp0 + 0x1000);

    DEBUG_INFO("initializing BSP LAPIC");
    lapic_init();

    initAPs();

    DEBUG_INFO("initializing scheduler");
    schedulerInit();

    createUserModeTask(userTaskACodeStart, userTaskACodeEnd, "taskA");
    createUserModeTask(userTaskBCodeStart, userTaskBCodeEnd, "taskB");

    DEBUG_INFO("scheduler ready, enabling interrupts");

    asm volatile("sti");
    while (1) {
        asm volatile("hlt");
    }
}