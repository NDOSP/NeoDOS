#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    VIDEO_ARGB,
    VIDEO_RGBA,
    VIDEO_ABGR,
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
    char signature[8];
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t rsdtAddress;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    char reserved[3];
} Rsdp;

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
} BootInfo;

extern BootInfo bInfo;

#endif
