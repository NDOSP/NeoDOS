#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

typedef struct {
    uint8_t ascii;
    void* bitmap;
} FontGlyph;

typedef struct {
    uint32_t version;
    uint32_t fontWidth;
    uint32_t fontHeight;
    uint32_t glyphCount;
    uint32_t bytesPerGlyph;
    FontGlyph glyphs[];
} FontInfo;

typedef struct {
    uint64_t addr;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t scanline;
    uint32_t format;
} FbInfo;

typedef struct {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
} FontSysInfo;

#endif
