#include "video.h"
#include "memory/memutils.h"

static FontGlyph* findGlyph(char c) {
    for (uint32_t i = 0; i < bInfo.font->glyphCount; i++) {
        if (bInfo.font->glyphs[i].ascii == (uint8_t)c) return &bInfo.font->glyphs[i];
    }

    for (uint32_t i = 0; i < bInfo.font->glyphCount; i++) {
        if (bInfo.font->glyphs[i].ascii == (uint8_t)'?')
            return &bInfo.font->glyphs[i];
    }

    return &bInfo.font->glyphs[0];
}

void putPixelScaled(uint32_t x, uint32_t y, VideoColor color) {
    for (uint32_t dy = 0; dy < bInfo.fontScale; dy++) {
        for (uint32_t dx = 0; dx < bInfo.fontScale; dx++) {
            putPixel(x + dx, y + dy, color);
        }
    }
}

void drawChar(char c, uint32_t x, uint32_t y, VideoColor color) {
    FontGlyph* glyph = findGlyph(c);
    if (!glyph) return;

    uint8_t* bmp = (uint8_t*)glyph->bitmap;
    uint32_t bytesPerRow = bInfo.font->bytesPerGlyph / bInfo.font->fontHeight;

    for (uint32_t row = 0; row < bInfo.font->fontHeight; row++) {
        for (uint32_t col = 0; col < bInfo.font->fontWidth; col++) {
            uint32_t byteIndex = row * bytesPerRow + (col / 8);
            uint8_t bitMask = 0x80 >> (col % 8); 
            if (bmp[byteIndex] & bitMask) {
                putPixelScaled(x + col * bInfo.fontScale, y + row * bInfo.fontScale, color);
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
        cursorX += bInfo.font->fontWidth * bInfo.fontScale;
        str++;
    }
}

static uint32_t Posx = 0;
static uint32_t Posy = 0;

void drawOutput(const char* str, VideoColor color) {
    if (!str) return;

    while (*str) {
        if (*str == '\n') {
            Posy += bInfo.font->fontHeight * bInfo.fontScale;
            Posx = 0;
            str++;
            continue;
        }

        drawChar(*str, Posx, Posy, color);
        Posx += bInfo.font->fontWidth * bInfo.fontScale;
        str++;

        if (Posy + bInfo.font->fontHeight * bInfo.fontScale > bInfo.fb.fbHeight) {
            uint32_t rowBytes = bInfo.fb.fbWidth * sizeof(uint32_t);
            uint32_t offset = bInfo.font->fontHeight * bInfo.fontScale * rowBytes;
            uint32_t size = (bInfo.fb.fbHeight * rowBytes) - offset;

            uint8_t* fb = (uint8_t*)bInfo.fb.fbPtr;

            memcpy(fb, fb + offset, size);

            memset(fb + size, 0, offset);
            Posy -= bInfo.font->fontHeight * bInfo.fontScale;
        }
    }
}

void SetCursorPos(uint32_t x, uint32_t y) {
    Posx = x;
    Posy = y;
}

static inline uint32_t colorToUint32(VideoColor c, int format) {
    switch (format) {
        case VIDEO_BGRA: return ((uint32_t)c.blue) | ((uint32_t)c.green << 8) | ((uint32_t)c.red  << 16);
        case VIDEO_RGBA: return ((uint32_t)c.red) | ((uint32_t)c.green << 8) | ((uint32_t)c.blue   << 16);
    }
    return 0;
}

void putPixel(uint32_t x, uint32_t y, VideoColor color) {
    if (x >= bInfo.fb.fbWidth) return;
    if (y >= bInfo.fb.fbHeight) return;

    uint32_t* fbPtr = (uint32_t*)bInfo.fb.fbPtr;
    uint64_t pos = x + y * bInfo.fb.fbWidth;
    fbPtr[pos] = colorToUint32(color, bInfo.fb.pixelFormat);
}

void cleanScreen(VideoColor color) {
    for (uint32_t y = 0; y < bInfo.fb.fbHeight; y++) {
        for (uint32_t x = 0; x < bInfo.fb.fbWidth; x++) {
            putPixel(x, y, color);
        }
    }
}

void drawHex64(uint64_t value, VideoColor color) {
    char buf[19]; 
    buf[0] = '0';
    buf[1] = 'x';

    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (value >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = (nibble < 10)
            ? ('0' + nibble)
            : ('A' + nibble - 10);
    }

    buf[18] = '\0';
    drawOutput(buf, color);
}

void drawHex8(uint8_t value, VideoColor color) {
    char buf[3];

    for (int i = 0; i < 2; i++) {
        uint8_t nibble = (value >> ((1 - i) * 4)) & 0xF;
        buf[i] = (nibble < 10)
            ? ('0' + nibble)
            : ('A' + nibble - 10);
    }

    buf[2] = '\0';
    drawOutput(buf, color);
}

void drawRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, VideoColor color) {
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            putPixel(x + col, y + row, color);
        }
    }
}

VideoColor black = { .blue = 0, .green = 0, .red = 0 };
VideoColor red = { .blue = 0, .green = 0, .red = 255 };
VideoColor blue = { .blue = 255, .green = 0, .red = 0 };
VideoColor green = { .blue = 0, .green = 255, .red = 0 };
VideoColor white = { .blue = 255, .green = 255, .red = 255 };
