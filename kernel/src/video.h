#ifndef VIDEO_H
#define VIDEO_H

#include "bootinfo.h"
#include <stdint.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} VideoColor;

extern VideoColor white;
extern VideoColor black;
extern VideoColor red;
extern VideoColor blue;
extern VideoColor green;

void putPixel(uint32_t x, uint32_t y, VideoColor color);
void drawChar(char c, uint32_t x, uint32_t y, VideoColor color);
void drawString(const char* str, uint32_t x, uint32_t y, VideoColor color);
void drawOutput(const char* str, VideoColor color);
void cleanScreen(VideoColor color);
void SetCursorPos(uint32_t x, uint32_t y);
void drawHex64(uint64_t value, VideoColor color);
void drawHex8(uint8_t value, VideoColor color);

#endif // VIDEO_H