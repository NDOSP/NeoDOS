#include "bootinfo.h"
#include "video.h"
#include "memory.h"
#include "interrupts/idt.h"
#include "tss.h"

extern void loadGdt(void);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    bootstrapInit();

    initTss();
    loadGdt();
    drawOutput("Hello, World!\n", white);
    
    idtInit();
    uint8_t* vaddr = (uint8_t*)addPageBootstrap(0xFFFFF000ULL, 0x10000, PAGE_PRESENT | PAGE_CACHE_DISABLE | PAGE_WRITE);
    vaddr[5] = 0xDE;
    
    asm volatile("ud2");
    asm volatile("hlt");
}