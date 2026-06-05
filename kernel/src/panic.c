#include "panic.h"
#include "interrupts/handlers.h"

void panic(char* msg) {
    handlerScreen();
    drawOutput("PANIC: ", white);
    drawOutput(msg, white);
    drawOutput("\n", white);

    asm volatile("hlt");
}