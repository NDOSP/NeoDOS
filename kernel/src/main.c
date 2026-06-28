#include "bootinfo.h"
#include "memory/vmm.h"
#include "memory/paging.h"
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
extern void jumpToUserMode(void*, void*);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

uint64_t _reg_timerHz = 337;

static void taskA(void) {
    serial_printf("TASK-A: started!\n");
    while (1) {
        for (volatile uint64_t i = 0; i < 2000000; i++) asm volatile("nop");
    }
}

static void taskB(void) {
    serial_printf("TASK-B: started!\n");
    while (1) {
        for (volatile uint64_t i = 0; i < 2000000; i++) asm volatile("nop");
    }
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
    createTask(taskA, "taskA");
    createTask(taskB, "taskB");
    DEBUG_INFO("scheduler ready, enabling interrupts");

    asm volatile("sti");
    while (1) {
        asm volatile("hlt");
    }
}