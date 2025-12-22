#include "nff.h"
#include "checksum.h"
#include "memory.h"

typedef struct __attribute__((packed)) {
    CHAR8 magic[4];
    UINT32 version;
    UINT16 width;
    UINT16 height;
    UINT32 charCount;
} NFF_HEADER;

typedef struct __attribute__((packed)) {
    UINT8 reserved1;
    UINT8 ascii;
    UINT8 reserved2[2];
    UINT64 fileOffset;
    UINT32 crc32;
} NFF_CHAR_ENTRY;

EFI_STATUS loadFont(CHAR16* fileAddress, FONT_INFO** font) {
    if (!font) return EFI_INVALID_PARAMETER;

    VOID* fileData;
    UINTN fileSize;
    EFI_STATUS Status = loadFile(fileAddress, &fileData, &fileSize);
    if (EFI_ERROR(Status)) return Status;

    if (fileSize < sizeof(NFF_HEADER)) return EFI_END_OF_FILE;

    UINT8* base = (UINT8*)fileData;
    NFF_HEADER* header = (NFF_HEADER*)base;

    if (header->magic[0] != 'N' || header->magic[1] != 'F' || header->magic[2] != 'F' || header->magic[3] != '\0') {
        return EFI_INCOMPATIBLE_VERSION;
    }

    UINT32 glyphCount = header->charCount;
    UINT32 bytesPerGlyph = header->height;

    UINTN headerSize = sizeof(NFF_HEADER);
    UINTN charTableOffset = (headerSize + 15) & ~15ULL;
    UINTN charTableSize = glyphCount * sizeof(NFF_CHAR_ENTRY);

    if (charTableOffset + charTableSize > fileSize) return EFI_LOAD_ERROR;

    NFF_CHAR_ENTRY* table = (NFF_CHAR_ENTRY*)(base + charTableOffset);

    UINTN fontStructSizePages = (sizeof(FONT_INFO) + glyphCount * sizeof(FONT_GLYPH) + glyphCount * bytesPerGlyph + PAGE_SIZE - 1) / PAGE_SIZE;

    FONT_INFO* outFont;
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, fontStructSizePages, (VOID**)&outFont);
    if (EFI_ERROR(Status)) return Status;

    UINT8* bitmapBase = (UINT8*)outFont + sizeof(FONT_INFO) + glyphCount * sizeof(FONT_GLYPH);

    outFont->version = header->version;
    outFont->fontWidth = header->width;
    outFont->fontHeight = header->height;
    outFont->glyphCount = glyphCount;
    outFont->bytesPerGlyph = bytesPerGlyph;

    for (UINT32 i = 0; i < glyphCount; i++) {
        NFF_CHAR_ENTRY* e = &table[i];

        if (e->fileOffset + bytesPerGlyph > fileSize)
            return EFI_LOAD_ERROR;

        VOID* src = base + e->fileOffset;
        UINT32 crc = crc32(src, bytesPerGlyph);
        if (crc != e->crc32)
            return EFI_CRC_ERROR;

        VOID* dst = bitmapBase + i * bytesPerGlyph;
        CopyMem(dst, src, bytesPerGlyph);

        outFont->glyphs[i].ascii = e->ascii;
        outFont->glyphs[i].bitmap = dst;
    }

    *font = outFont;
    return EFI_SUCCESS;
}