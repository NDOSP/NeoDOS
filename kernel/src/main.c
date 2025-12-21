#include "bootinfo.h"
#include "video.h"

__attribute__((section(".bootinfo"))) BootInfo bInfo;

void kmain() {
    initVideo(bInfo.fb);
    VideoColor red = { .red = 255, .green = 0, .blue = 0 };
    for (uint32_t y = 0; y < bInfo.fb.fbHeight; y++) {
        for (uint32_t x = 0; x < bInfo.fb.fbWidth; x++) {
            putPixel(x, y, red);
        }
    }
    asm volatile("hlt");
}