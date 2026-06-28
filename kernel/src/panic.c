#include "panic.h"
#include "interrupts/handlers.h"
#include "serial.h"

void panic(char* msg) {
    serial_printf("ERROR: (kernel) PANIC: %s\n", msg);

    handlerScreen();
    drawOutput("PANIC: ", white);
    drawOutput(msg, white);
    drawOutput("\n", white);

    asm volatile("hlt");
}