#include "bootinfo.h"

__attribute__((section(".bootinfo"))) BootInfo bInfo;

const char* test = "Hello, World!";

void kmain() {
    asm volatile("hlt");
}