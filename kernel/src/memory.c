#include "memory.h"
#include "bootinfo.h"

void* memcpy(void* dest, const void* src, uint32_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* s, int c, uint32_t n) {
    uint8_t *p = s;
    for (uint32_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

typedef struct {
    void* address;
    uint8_t isFree : 1;
} BootstrapMemoryMetadata;

void bootstrapInit(void) {
    uint8_t* base = (uint8_t*)bInfo.bootstrapMemoryAddress;
    size_t dataPages = bInfo.bootstrapMemoryPages - 1;

    BootstrapMemoryMetadata* metadata = (BootstrapMemoryMetadata*)base;

    for (size_t i = 0; i < dataPages; i++) {
        metadata[i].address = base + PAGE_SIZE * (i + 1);
        metadata[i].isFree = 1;
    }
}

__attribute__((noinline))
void* bootstrapAllocate(void) {
    uint8_t* base = (uint8_t*)bInfo.bootstrapMemoryAddress;
    BootstrapMemoryMetadata* metadata = (BootstrapMemoryMetadata*)base;

    size_t dataPages = bInfo.bootstrapMemoryPages - 1;

    for (size_t i = 0; i < dataPages; i++) {
        if (metadata[i].isFree) {
            metadata[i].isFree = 0;
            return metadata[i].address;
        }
    }

    return NULL;
}

void bootstrapFree(void* ptr) {
    uint8_t* base = (uint8_t*)bInfo.bootstrapMemoryAddress;
    BootstrapMemoryMetadata* metadata = (BootstrapMemoryMetadata*)base;

    size_t dataPages = bInfo.bootstrapMemoryPages - 1;

    for (size_t i = 0; i < dataPages; i++) {
        if (metadata[i].address == ptr) {
            metadata[i].isFree = 1;
            return;
        }
    }
}

static inline void refreshTLB(void) {
    uint64_t cr3;
    asm volatile (
        "mov %%cr3, %0\n"
        "mov %0, %%cr3"
        : "=r"(cr3)
        :
        : "memory"
    );
}

void* addPageBootstrap(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (vaddr % PAGE_SIZE != 0) return NULL;
    if (paddr % PAGE_SIZE != 0) return NULL;

    uint16_t pml4_i = PML4_IDX(vaddr);
    uint16_t pdpt_i = PDPT_IDX(vaddr);
    uint16_t pd_i   = PD_IDX(vaddr);
    uint16_t pt_i   = PT_IDX(vaddr);

    uint64_t* pml4 = (uint64_t*)PML4_VADDR;

    if (!(pml4[pml4_i] & PAGE_PRESENT)) {
        void* new = bootstrapAllocate();
        if (!new) return NULL;
        memset(new, 0, PAGE_SIZE);

        pml4[pml4_i] = ((uint64_t)new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
        refreshTLB();
    }

    uint64_t* pdpt = PDPT_VADDR(pml4_i);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        void* new = bootstrapAllocate();
        if (!new) return NULL;
        memset(new, 0, PAGE_SIZE);
    
        pdpt[pdpt_i] = ((uint64_t)new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
        refreshTLB();
    }
    
    uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
    
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        void* new = bootstrapAllocate();
        if (!new) return NULL;
        memset(new, 0, PAGE_SIZE);
    
        pd[pd_i] = ((uint64_t)new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
        refreshTLB();
    }
 
    uint64_t* pt = PT_VADDR(pml4_i, pdpt_i, pd_i);
    pt[pt_i] = (paddr & ENTRY_ADDR_MASK) | flags;
 
    refreshTLB();
    return (void*)vaddr;
}
