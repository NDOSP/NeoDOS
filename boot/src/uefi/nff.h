#ifndef NFF_BOOT_H
#define NFF_BOOT_H

#include "file.h"

typedef struct {
    UINT8 ascii;
    VOID* bitmap;
} FONT_GLYPH;

typedef struct {
    UINT32 version;
    UINT32 fontWidth;
    UINT32 fontHeight;
    UINT32 glyphCount;
    UINT32 bytesPerGlyph;
    FONT_GLYPH glyphs[];
} FONT_INFO;

EFI_STATUS loadFont(CHAR16* fileAddress, FONT_INFO** font);

#endif // NFF_BOOT_H