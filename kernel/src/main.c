#include "bootinfo.h"
#include "video.h"
#include "memory.h"
#include "idt.h"

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    initVideo(&bInfo);
    drawString(" !\"#$%&'()*+,-./", 0, 0, white);
    drawString("0123456789:;<=>?", 0, bInfo.font->fontHeight * bInfo.fontScale * 1, white);
    drawString("@ABCDEFGHIJKLMNO", 0, bInfo.font->fontHeight * bInfo.fontScale * 2, white);
    drawString("PQRSTUVWXYZ[\\]^_", 0, bInfo.font->fontHeight * bInfo.fontScale * 3, white);
    drawString("`abcdefghijklmno", 0, bInfo.font->fontHeight * bInfo.fontScale* 4, white);
    drawString("pqrstuvwxyz{|}~ ", 0, bInfo.font->fontHeight * bInfo.fontScale* 5, white);
    idtInit();
    asm volatile("ud2");
    asm volatile("hlt");
}