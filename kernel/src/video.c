#include "video.h"

VideoFramebuffer fbData;
FontInfo* font;
uint64_t scale;

void initVideo(BootInfo* bInfo) {
    fbData = bInfo->fb;
    font = bInfo->font;
    scale = bInfo->fontScale;
}

static FontGlyph* findGlyph(char c) {
    for (uint32_t i = 0; i < font->glyphCount; i++) {
        if (font->glyphs[i].ascii == (uint8_t)c) return &font->glyphs[i];
    }

    for (uint32_t i = 0; i < font->glyphCount; i++) {
        if (font->glyphs[i].ascii == (uint8_t)'?')
            return &font->glyphs[i];
    }

    return &font->glyphs[0];
}

void putPixelScaled(uint32_t x, uint32_t y, VideoColor color) {
    for (uint32_t dy = 0; dy < scale; dy++) {
        for (uint32_t dx = 0; dx < scale; dx++) {
            putPixel(x + dx, y + dy, color);
        }
    }
}

void drawChar(char c, uint32_t x, uint32_t y, VideoColor color) {
    FontGlyph* glyph = findGlyph(c);
    if (!glyph) return;

    uint8_t* bmp = (uint8_t*)glyph->bitmap;
    uint32_t bytesPerRow = font->bytesPerGlyph / font->fontHeight;

    for (uint32_t row = 0; row < font->fontHeight; row++) {
        for (uint32_t col = 0; col < font->fontWidth; col++) {
            uint32_t byteIndex = row * bytesPerRow + (col / 8);
            uint8_t bitMask = 0x80 >> (col % 8); 
            if (bmp[byteIndex] & bitMask) {
                putPixelScaled(x + col * scale, y + row * scale, color);
            }
        }
    }
}

void drawString(const char* str, uint32_t x, uint32_t y, VideoColor color) {
    if (!str) return;

    uint32_t cursorX = x;
    uint32_t cursorY = y;

    while (*str) {
        drawChar(*str, cursorX, cursorY, color);
        cursorX += font->fontWidth * scale;
        str++;
    }
}

static inline uint32_t colorToUint32(VideoColor c, int format) {
    switch (format) {
        case VIDEO_BGRA: return ((uint32_t)c.blue) | ((uint32_t)c.green << 8) | ((uint32_t)c.red  << 16);
        case VIDEO_RGBA: return ((uint32_t)c.red) | ((uint32_t)c.green << 8) | ((uint32_t)c.blue   << 16);
    }
    return 0;
}

void putPixel(uint32_t x, uint32_t y, VideoColor color) {
    if (x >= fbData.fbWidth) return;
    if (y >= fbData.fbHeight) return;

    uint32_t* fbPtr = (uint32_t*)fbData.fbPtr;
    uint64_t pos = x + y * fbData.fbWidth;
    fbPtr[pos] = colorToUint32(color, fbData.pixelFormat);
}

void cleanScreen(VideoColor color) {
    for (uint32_t y = 0; y < fbData.fbHeight; y++) {
        for (uint32_t x = 0; x < fbData.fbWidth; x++) {
            putPixel(x, y, color);
        }
    }
}


VideoColor black = { .blue = 0, .green = 0, .red = 0 };
VideoColor red = { .blue = 0, .green = 0, .red = 255 };
VideoColor blue = { .blue = 255, .green = 0, .red = 0 };
VideoColor green = { .blue = 0, .green = 255, .red = 0 };
VideoColor white = { .blue = 255, .green = 255, .red = 255 };
