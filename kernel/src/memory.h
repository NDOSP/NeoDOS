#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define KB(x) ((x) * 1024ULL)
#define MB(x) (KB(x) * 1024ULL)
#define GB(x) (MB(x) * 1024ULL)

#define PAGE_SIZE 4096
#define RECURSIVE_SLOT 505ULL

#define CANONICAL(addr) (((int64_t)((uint64_t)(addr) << 16) >> 16))

#define PML4_VADDR ((uint64_t*)CANONICAL(((uint64_t)RECURSIVE_SLOT << 39) | ((uint64_t)RECURSIVE_SLOT << 30) | ((uint64_t)RECURSIVE_SLOT << 21) | ((uint64_t)RECURSIVE_SLOT << 12)))
#define PDPT_VADDR(pml4_i) ((uint64_t*)CANONICAL(((uint64_t)RECURSIVE_SLOT << 39) | ((uint64_t)RECURSIVE_SLOT << 30) | ((uint64_t)RECURSIVE_SLOT << 21) | ((uint64_t)(pml4_i) << 12)))
#define PD_VADDR(pml4_i, pdpt_i) ((uint64_t*)CANONICAL(((uint64_t)RECURSIVE_SLOT << 39) | ((uint64_t)RECURSIVE_SLOT << 30) | ((uint64_t)(pml4_i) << 21) | ((uint64_t)(pdpt_i) << 12)))
#define PT_VADDR(pml4_i, pdpt_i, pd_i) ((uint64_t*)CANONICAL(((uint64_t)RECURSIVE_SLOT << 39) | ((uint64_t)(pml4_i) << 30) | ((uint64_t)(pdpt_i) << 21) | ((uint64_t)(pd_i) << 12)))

#define PAGE_PRESENT            (1ULL << 0)
#define PAGE_WRITE              (1ULL << 1)
#define PAGE_USER               (1ULL << 2)
#define PAGE_WRITE_THROUGH      (1ULL << 3)
#define PAGE_CACHE_DISABLE      (1ULL << 4)
#define PAGE_ACCESSED           (1ULL << 5)
#define PAGE_DIRTY              (1ULL << 6)
#define PAGE_PAGE_SIZE          (1ULL << 7)
#define PAGE_GLOBAL             (1ULL << 8)
#define PAGE_EXEC_DISABLE       (1ULL << 63)

#define MAX_IDX 0x1FF
#define PML4_IDX(x)  (((x) >> 39) & MAX_IDX)
#define PDPT_IDX(x)  (((x) >> 30) & MAX_IDX)
#define PD_IDX(x)    (((x) >> 21) & MAX_IDX)
#define PT_IDX(x)    (((x) >> 12) & MAX_IDX)

#define ENTRY_ADDR_MASK          0x000FFFFFFFFFF000ULL

#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define PAGE_ALIGN_UP(size) ALIGN_UP(size, PAGE_SIZE)
#define ALIGN_DOWN(size, align) ((size) & ~((align) - 1))
#define PAGE_ALIGN_DOWN(size) ALIGN_DOWN(size, PAGE_SIZE)

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

void* memset(void* s, int c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);

void bootstrapInit(void);
void* addPageBootstrap(uint64_t vaddr, uint64_t paddr, uint64_t flags);
void* addPageRangeBootstrap(uint64_t vaddr, size_t size, uint64_t paddr, uint64_t flags);

#endif // MEMORY_H