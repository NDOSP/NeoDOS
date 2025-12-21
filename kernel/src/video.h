#ifndef VIDEO_H
#define VIDEO_H

#include "bootinfo.h"
#include <stdint.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} VideoColor;

void initVideo(VideoFramebuffer fb);
void putPixel(uint32_t x, uint32_t y, VideoColor color);

#endif // VIDEO_H