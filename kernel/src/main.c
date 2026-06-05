#include "bootinfo.h"
#include "video.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "interrupts/idt.h"
#include "tss.h"
#include "acpi.h"
#include "panic.h"
#include "syscalls/syscalls.h"

extern void loadGdt(uint64_t);
extern void jumpToUserMode(void*, void*);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void user_test() {
    asm volatile(
        "mov $1, %%rax\n"
        "syscall\n"
        : : : "rax", "rcx", "r11"
    );

    while (1) {
        asm volatile("nop");
    }
}

void kmain() {
    initTss();
    loadGdt(0);
    cleanScreen(black);
    drawOutput("Hello, World!\n", white);
    
    idtInit();
    acpiInit();
    void* myKstack = allocatePages(4, PAGE_PRESENT | PAGE_WRITE);
    initSyscalls(0, (void*)tss[0].rsp0 + 0x1000);
    
    void* userStack = allocatePages(2, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    void* userCode = (void*)0x1000000;
    addPage((uint64_t)userCode, (uint64_t)vmtoPm((uint64_t)user_test), PAGE_PRESENT | PAGE_USER);
    jumpToUserMode(userCode, (void*)((uint64_t)userStack + 2 * PAGE_SIZE));
}