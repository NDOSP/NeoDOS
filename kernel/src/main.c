#include "bootinfo.h"

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    asm volatile("ud2");
    asm volatile("hlt");
}