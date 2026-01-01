#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>
#include <stddef.h>
#include "acpi.h"

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

typedef enum {
    VIDEO_RGBA,
    VIDEO_BGRA
} VideoType;

typedef struct {
    void* fbPtr;
    size_t fbSize;
    uint32_t fbWidth;
    uint32_t fbHeight;
    uint32_t fbScanlineBytes;
    VideoType pixelFormat;
} VideoFramebuffer;

typedef struct {
    uint64_t paddr;
    uint64_t vaddr;
    uint64_t size;
    uint32_t flags;
} MappingInfo;

typedef struct {
    uint64_t entryPoint;
    MappingInfo segmentMapping[255];
    uint8_t segmentCount;
    MappingInfo bootInfo;
} KernelInfo;

typedef struct {
    uint64_t address;
    uint64_t size;
    uint32_t type;
    uint32_t reserved[3];
} MemoryMapEntry;

#define MEMORY_MAP_MAX_ENTRIES (4096 / sizeof(MemoryMapEntry))

typedef struct {
    size_t numberOfEntries;
    size_t mapKey;
    size_t descriptorSize;
    size_t descriptorVersion;
    MemoryMapEntry entries[MEMORY_MAP_MAX_ENTRIES] __attribute__((aligned(4096)));
} MemoryMap;

typedef struct {
    uint64_t* pml4;
    uint64_t stackCount;
    VideoFramebuffer fb;
    KernelInfo kInfo;
    Rsdp* rsdp;
    FontInfo* font;
    uint64_t fontScale;
    void* bootstrapMemoryAddress;
    uint64_t bootstrapMemoryPages;
    MemoryMap map;
} BootInfo;

extern BootInfo bInfo;

#endif
