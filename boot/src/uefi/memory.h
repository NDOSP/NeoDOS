#ifndef MEMORY_BOOT_H
#define MEMORY_BOOT_H

#include <efi.h>
#include <efilib.h>

#define KB(x) x*1024
#define MB(x) KB(x)*1024

#define PAGE_SIZE 4096

#define PAGE_PRESENT 0x001
#define PAGE_RW      0x002
#define PAGE_USER    0x004
#define PAGE_PWT     0x008
#define PAGE_PCD     0x010
#define PAGE_ACCESSED 0x020
#define PAGE_DIRTY   0x040
#define PAGE_PSE     0x080 
#define PAGE_GLOBAL  0x100
#define PAGE_NX      (1ULL << 63)

UINTN align_up(UINTN size, UINTN align);
EFI_STATUS addPage(UINT64* pml4, UINT64 vaddr, UINT64 paddr, UINT64 flags);
EFI_STATUS initPage(UINT64** address);

#endif // MEMORY_BOOT_H