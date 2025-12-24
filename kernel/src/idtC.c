#include "idt.h"
#include "video.h"

extern void idtDefaultStub(void);
extern void idt0Stub(void);
extern void idt1Stub(void);
extern void idt2Stub(void);
extern void idt3Stub(void);
extern void idt4Stub(void);
extern void idt5Stub(void);
extern void idt6Stub(void);
extern void idt7Stub(void);
extern void idt8Stub(void);
extern void idt9Stub(void);
extern void idt10Stub(void);
extern void idt11Stub(void);
extern void idt12Stub(void);
extern void idt13Stub(void);
extern void idt14Stub(void);
extern void idt15Stub(void);
extern void idt16Stub(void);
extern void idt17Stub(void);
extern void idt18Stub(void);
extern void idt19Stub(void);
extern void idt20Stub(void);
extern void idt21Stub(void);

__attribute__((aligned(16))) static IdtEntry idt[256];
static IdtPtr idtp = { .limit = sizeof(idt) - 1, .base = (uint64_t)&idt[0] };

void idtLoad(void) {
    asm volatile("lidt %0" : : "m"(idtp));
}

void idtSetEntry(uint8_t num, void (*handler)(void), uint8_t type_attr, uint8_t ist) {
    uint64_t addr = (uint64_t)handler;

    idt[num].offset_low = addr & 0xFFFF;
    idt[num].selector = 0x08;
    idt[num].ist = ist & 0x7;
    idt[num].type_attr = type_attr;
    idt[num].offset_mid = (addr >> 16) & 0xFFFF;
    idt[num].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
}

void idtInit(void) {
    for (int i = 0; i < 32; i++) {
        switch (i) {
            case 0: idtSetEntry(0, idt0Stub, 0x8E, 0); break;
            case 1: idtSetEntry(1, idt1Stub, 0x8E, 0); break;
            case 2: idtSetEntry(2, idt2Stub, 0x8E, 0); break;
            case 3: idtSetEntry(3, idt3Stub, 0x8E, 0); break;
            case 4: idtSetEntry(4, idt4Stub, 0x8E, 0); break;
            case 5: idtSetEntry(5, idt5Stub, 0x8E, 0); break;
            case 6: idtSetEntry(6, idt6Stub, 0x8E, 0); break;
            case 7: idtSetEntry(7, idt7Stub, 0x8E, 0); break;
            case 8: idtSetEntry(8, idt8Stub, 0x8E, 1); break;
            case 9: idtSetEntry(9, idt9Stub, 0x8E, 0); break;
            case 10: idtSetEntry(10, idt10Stub, 0x8E, 0); break;
            case 11: idtSetEntry(11, idt11Stub, 0x8E, 0); break;
            case 12: idtSetEntry(12, idt12Stub, 0x8E, 0); break;
            case 13: idtSetEntry(13, idt13Stub, 0x8E, 0); break;
            case 14: idtSetEntry(14, idt14Stub, 0x8E, 2); break;
            case 15: idtSetEntry(15, idt15Stub, 0x8E, 0); break;
            case 16: idtSetEntry(16, idt16Stub, 0x8E, 0); break;
            case 17: idtSetEntry(17, idt17Stub, 0x8E, 0); break;
            case 18: idtSetEntry(18, idt18Stub, 0x8E, 0); break;
            case 19: idtSetEntry(19, idt19Stub, 0x8E, 0); break;
            case 20: idtSetEntry(20, idt20Stub, 0x8E, 0); break;
            case 21: idtSetEntry(21, idt21Stub, 0x8E, 0); break;
            default: idtSetEntry(i, idtDefaultStub, 0x8E, 0); break;
        }
    }

    idtLoad();
}

void idtHandler(INTERRUPT_FRAME* frame) {
    cleanScreen(red);
    char buffer[4] = {'I', '0' + (frame->interruptNumber / 10), '0' + (frame->interruptNumber % 10), '\0'};
    drawString(buffer, 0, 0, white);

    if (frame->interruptNumber < 32) {
        asm volatile("hlt");
    }
}