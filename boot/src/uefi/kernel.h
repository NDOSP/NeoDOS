#ifndef KERNEL_BOOT_H
#define KERNEL_BOOT_H

#include <efi.h>
#include <efilib.h>
#include <elf.h>

typedef struct {
    UINT64 paddr;
    UINT64 vaddr;
    UINT64 size;
    UINT32 flags;
} MAPPING_INFO;

typedef struct {
    UINT64 entryPoint;
    MAPPING_INFO segmentMapping[255];
    UINT8 segmentCount;
    MAPPING_INFO bootInfo;
} KERNEL_INFO;

EFI_STATUS loadElf(CHAR16* path, KERNEL_INFO* info);

#endif // KERNEL_BOOT_H