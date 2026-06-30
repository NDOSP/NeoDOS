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
#include "modman/modman.h"
#include "shm/shm.h"

extern void loadGdt(uint64_t);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    serial_init();

    initTss();
    loadGdt(0);
    DEBUG_INFO("Hello, NeoDOS kernel booting");

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

    DEBUG_INFO("initializing module manager");
    modman_init();
    shm_init();

    if (bInfo.initEntry) {
        size_t stack_pages = 4;
        void* stack_phys = pmmAllocator(stack_pages);
        if (stack_phys) {
            Task* task = createUserTaskPrio((void (*)(void))(uint64_t)bInfo.initEntry, "init", PRIORITY_HIGH);
            if (task) {
                vmm_map_in_cr3(task->cr3, (uint64_t)stack_phys, stack_pages * PAGE_SIZE,
                               (uint64_t)stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                task->frame.rsp = (uint64_t)stack_phys + stack_pages * PAGE_SIZE;
                addTaskToReadyQueue(task);
                DEBUG_INFO("INIT: task created entry=%lX stack=%lX",
                    bInfo.initEntry, task->frame.rsp);
            }
        }
    }

    DEBUG_INFO("launching modules");
    launchModules();

    DEBUG_INFO("scheduler ready, enabling interrupts");

    asm volatile("sti");
    while (1) {
        asm volatile("hlt");
    }
}