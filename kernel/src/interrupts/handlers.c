#include "handlers.h"
#include "video.h"

static InterruptHandler handlers[256];

void registerInterruptHandler(uint8_t n, InterruptHandler h) {
    handlers[n] = h;
}

InterruptHandler getInterruptHandler(uint8_t n) {
    return handlers[n];
}

void defaultHandler(INTERRUPT_FRAME* frame) {
    cleanScreen(red);
    drawRect(0, 0, bInfo.fb.fbWidth, bInfo.font->fontHeight * bInfo.fontScale, white);
    drawString("NeoDOS Error", (bInfo.fb.fbWidth - strlen("NeoDOS Error") * bInfo.font->fontWidth * bInfo.fontScale) / 2, 0, red);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 2);
    drawOutput("RAX: ", white);
    drawHex64(frame->rax, white);
    drawOutput(" RBX: ", white);
    drawHex64(frame->rbx, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 3);
    drawOutput("RCX: ", white);
    drawHex64(frame->rcx, white);
    drawOutput(" RDX: ", white);
    drawHex64(frame->rdx, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 4);
    drawOutput("RSI: ", white);
    drawHex64(frame->rsi, white);
    drawOutput(" RDI: ", white);
    drawHex64(frame->rdi, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 5);
    drawOutput("RBP: ", white);
    drawHex64(frame->rbp, white);
    drawOutput(" RSP: ", white);
    drawHex64(frame->rsp, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 6);
    drawOutput("R8:  ", white);
    drawHex64(frame->r8, white);
    drawOutput(" R9:  ", white);
    drawHex64(frame->r9, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 7);
    drawOutput("R10: ", white);
    drawHex64(frame->r10, white);
    drawOutput(" R11: ", white);
    drawHex64(frame->r11, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 8);
    drawOutput("R12: ", white);
    drawHex64(frame->r12, white);
    drawOutput(" R13: ", white);
    drawHex64(frame->r13, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 9);
    drawOutput("R14: ", white);
    drawHex64(frame->r14, white);
    drawOutput(" R15: ", white);
    drawHex64(frame->r15, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 10);
    drawOutput("RIP: ", white);
    drawHex64(frame->rip, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 11);
    drawOutput("CS:  ", white);
    drawHex64(frame->cs, white);
    drawOutput(" SS:  ", white);
    drawHex64(frame->ss, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 12);
    drawOutput("RLF: ", white);
    drawHex64(frame->rflags, white);

    SetCursorPos((bInfo.fb.fbWidth - 47 * bInfo.fontScale * bInfo.font->fontWidth) / 2, bInfo.fontScale * bInfo.font->fontHeight * 13);
    drawOutput("ERR: ", white);
    drawHex64(frame->error, white);
    drawOutput(" INT: ", white);
    drawHex64(frame->interruptNumber, white);
    drawOutput("\n", white);
    drawString("#", bInfo.fb.fbWidth - bInfo.font->fontWidth * bInfo.fontScale, bInfo.fb.fbHeight - bInfo.font->fontHeight * bInfo.fontScale, white);
}


void pageFaultHandler(INTERRUPT_FRAME* frame) {
    defaultHandler(frame);
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    drawOutput("#PF at ", white);
    drawHex64(cr2, white);
    drawOutput("\n", white);

    asm volatile("hlt");
}