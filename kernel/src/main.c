#include "bootinfo.h"
#include "video.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "interrupts/idt.h"
#include "tss.h"
#include "acpi.h"
#include "panic.h"

extern void loadGdt(void);

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    initTss();
    loadGdt();
    cleanScreen(black);
    drawOutput("Hello, World!\n", white);
    
    idtInit();
    acpiInit();
    panic("Reached end of kmain");
    asm volatile("hlt");
}