#include "bootinfo.h"
#include "video.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "interrupts/idt.h"
#include "tss.h"
#include "acpi.h"
#include "panic.h"

extern void loadGdt(uint64_t);
extern void jumpToUserMode(void*, void*);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void user_test() {
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
    
    void* userStack = allocatePages(2, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    void* userCode = (void*)0x1000000;
    addPage((uint64_t)userCode, (uint64_t)vmtoPm((uint64_t)user_test), PAGE_PRESENT | PAGE_USER);
    jumpToUserMode(userCode, (void*)((uint64_t)userStack + 2 * PAGE_SIZE));
}