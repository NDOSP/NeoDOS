#include "handlers.h"
#include "serial.h"
#include "scheduler/scheduler.h"

static InterruptHandler handlers[256];

void registerInterruptHandler(uint8_t n, InterruptHandler h) {
    handlers[n] = h;
}

InterruptHandler getInterruptHandler(uint8_t n) {
    return handlers[n];
}

void defaultHandler(INTERRUPT_FRAME* frame) {
    serial_printf("ERROR: (kernel) Unhandled exception INT=%lX ERR=%lX at RIP=%lX\n",
        frame->interruptNumber, frame->error, frame->rip);
    serial_printf("RAX: %lX  RBX: %lX\n", frame->rax, frame->rbx);
    serial_printf("RCX: %lX  RDX: %lX\n", frame->rcx, frame->rdx);
    serial_printf("RSI: %lX  RDI: %lX\n", frame->rsi, frame->rdi);
    serial_printf("RBP: %lX  RSP: %lX\n", frame->rbp, frame->rsp);
    serial_printf("R8:  %lX  R9:  %lX\n", frame->r8, frame->r9);
    serial_printf("R10: %lX  R11: %lX\n", frame->r10, frame->r11);
    serial_printf("R12: %lX  R13: %lX\n", frame->r12, frame->r13);
    serial_printf("R14: %lX  R15: %lX\n", frame->r14, frame->r15);
    serial_printf("RIP: %lX  CS:  %lX  SS:  %lX\n", frame->rip, frame->cs, frame->ss);
    serial_printf("RFLAGS: %lX  ERR: %lX  INT: %lX\n", frame->rflags, frame->error, frame->interruptNumber);
}

void pageFaultHandler(INTERRUPT_FRAME* frame) {
    defaultHandler(frame);
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    serial_printf("ERROR: (kernel) #PF at %lX\n", cr2);

    if (frame->cs == 0x2B) {
        Task* t = getCurrentTask();
        serial_printf("TASK: killing task '%s' (pid=%lu) due to page fault\n", t->name, t->id);
        killTaskAndSwitch(frame);
    } else {
        asm volatile("hlt");
        while (1);
    }
}