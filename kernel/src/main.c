#include "bootinfo.h"
#include "video.h"
#include "memory.h"

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    initVideo(&bInfo);
    VideoColor white = { .red = 255, .green = 255, .blue = 255 };
    drawString("ABCDEFGHIJKL", 0, 0, white);
    drawString("MNOPRSTUWXYZ", 0, bInfo.font->fontHeight * 2, white);
    drawString("abcdefghijkl", 0, bInfo.font->fontHeight * 4, white);
    drawString("mnoprstuwxyz", 0, bInfo.font->fontHeight * 6, white);
    drawString("1234567890", 0, bInfo.font->fontHeight * 8, white);
    asm volatile("hlt");
}