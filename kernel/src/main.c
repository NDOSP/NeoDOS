#include "bootinfo.h"
#include "video.h"
#include "memory.h"
#include "idt.h"
#include "tss.h"

extern void loadGdt(void);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    initTss();
    loadGdt();
    drawOutput("Hello, World!\n", white);
    
    idtInit();
    asm volatile("ud2");
    asm volatile("hlt");
}