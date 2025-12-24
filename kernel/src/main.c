#include "bootinfo.h"
#include "video.h"
#include "memory.h"
#include "idt.h"

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    drawOutput("Hello, World!\n", white);
    
    idtInit();
    asm volatile("hlt");
}