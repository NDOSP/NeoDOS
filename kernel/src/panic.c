#include "panic.h"
#include "serial.h"

void panic(char* msg) {
    serial_printf("ERROR: (kernel) PANIC: %s\n", msg);
    asm volatile("hlt");
}