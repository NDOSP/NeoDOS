#ifndef MEMORY_BOOT_H
#define MEMORY_BOOT_H

#include <efi.h>
#include <efilib.h>

#define KB(x) x*1024
#define MB(x) KB(x)*1024

#define PAGE_SIZE 4096

#define ENTRY_PRESENT            (1ULL << 0)
#define ENTRY_RW                 (1ULL << 1)
#define ENTRY_USER               (1ULL << 2)
#define ENTRY_WRITE_THROUGH      (1ULL << 3)
#define ENTRY_CACHE_DISABLE      (1ULL << 4)
#define ENTRY_ACCESSED           (1ULL << 5)
#define ENTRY_DIRTY              (1ULL << 6)
#define ENTRY_PAGE_SIZE          (1ULL << 7)
#define ENTRY_GLOBAL             (1ULL << 8)
#define ENTRY_EXEC_DISABLE       (1ULL << 63)

#define MAX_IDX 0x1FF
#define PML4_IDX(x)  (((x) >> 39) & MAX_IDX)
#define PDPT_IDX(x)  (((x) >> 30) & MAX_IDX)
#define PD_IDX(x)    (((x) >> 21) & MAX_IDX)
#define PT_IDX(x)    (((x) >> 12) & MAX_IDX)

#define ENTRY_ADDR_MASK          0x000FFFFFFFFFF000ULL

typedef UINT64 PAGETABLEENTRY;

UINTN align_up(UINTN size, UINTN align);
EFI_STATUS addPage(PAGETABLEENTRY (*pml4)[512], UINT64 vaddr, UINT64 paddr, UINT64 flags);
EFI_STATUS initPage(PAGETABLEENTRY (**address)[512]);

typedef struct {
    UINT64 address;
    UINT64 size;
    UINT32 type;
    UINT32 reserved[3];
} MEMORY_MAP_ENTRY; 

typedef struct {
    UINTN numberOfEntries;
    UINTN mapKey;
    UINTN descriptorSize;
    UINTN descriptorVersion;
    MEMORY_MAP_ENTRY entries[4096 / sizeof(MEMORY_MAP_ENTRY)] __attribute__((aligned(4096)));
} MEMORY_MAP;

EFI_STATUS getMemoryMap(MEMORY_MAP* map);

#endif // MEMORY_BOOT_H