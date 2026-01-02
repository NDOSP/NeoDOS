#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include <stdbool.h>

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

void* addPage(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (vaddr % PAGE_SIZE != 0) return NULL;
    if (paddr % PAGE_SIZE != 0) return NULL;

    uint16_t pml4_i = PML4_IDX(vaddr);
    uint16_t pdpt_i = PDPT_IDX(vaddr);
    uint16_t pd_i   = PD_IDX(vaddr);
    uint16_t pt_i   = PT_IDX(vaddr);

    uint64_t* pml4 = (uint64_t*)PML4_VADDR;

    if (!(pml4[pml4_i] & PAGE_PRESENT)) {
        void* new = pmmAllocator(1);
        if (!new) return NULL;
        memset(new, 0, PAGE_SIZE);

        pml4[pml4_i] = ((uint64_t)new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
        refreshTLB();
    }

    uint64_t* pdpt = PDPT_VADDR(pml4_i);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        void* new = pmmAllocator(1);
        if (!new) return NULL;
        memset(new, 0, PAGE_SIZE);
    
        pdpt[pdpt_i] = ((uint64_t)new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
        refreshTLB();
    }
    
    uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
    
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        void* new = pmmAllocator(1);
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

static inline bool pageTableEmpty(uint64_t* table) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & PAGE_PRESENT)
            return false;
    }
    return true;
}

void freePage(uint64_t vaddr) {
    if (vaddr % PAGE_SIZE != 0) return;

    uint16_t pml4_i = PML4_IDX(vaddr);
    uint16_t pdpt_i = PDPT_IDX(vaddr);
    uint16_t pd_i = PD_IDX(vaddr);
    uint16_t pt_i = PT_IDX(vaddr);

    uint64_t* pml4 = (uint64_t*)PML4_VADDR;
    if (!(pml4[pml4_i] & PAGE_PRESENT)) return;

    uint64_t* pdpt = PDPT_VADDR(pml4_i);
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return;

    uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
    if (!(pd[pd_i] & PAGE_PRESENT)) return;

    uint64_t* pt = PT_VADDR(pml4_i, pdpt_i, pd_i);
    if (!(pt[pt_i] & PAGE_PRESENT)) return;

    uint64_t paddr = pt[pt_i] & ENTRY_ADDR_MASK;
    pt[pt_i] = 0;
    pmmFree((void*)paddr, 1);
    refreshTLB();

    if (pageTableEmpty(pt)) {
        pmmFree((void*)(pd[pd_i] & ENTRY_ADDR_MASK), 1);
        pd[pd_i] = 0;
    } else return;

    if (pageTableEmpty(pd)) {
        pmmFree((void*)(pdpt[pdpt_i] & ENTRY_ADDR_MASK), 1);
        pdpt[pdpt_i] = 0;
    } else return;

    if (pageTableEmpty(pdpt)) {
        pmmFree((void*)(pml4[pml4_i] & ENTRY_ADDR_MASK), 1);
        pml4[pml4_i] = 0;
    }
}

void* addPageRange(uint64_t vaddr, size_t size, uint64_t paddr, uint64_t flags) {
    size_t pages = PAGE_ALIGN_UP(size) / PAGE_SIZE;
    for (size_t i = 0; i < pages; i++) {
        addPage(PAGE_ALIGN_DOWN(vaddr) + i * PAGE_SIZE, PAGE_ALIGN_DOWN(paddr) + i * PAGE_SIZE, flags);
    }

    return (void*)vaddr;
}

void* allocatePages(size_t numOfPages, uint64_t flags) {
    void* addr = pmmAllocator(numOfPages);
    if (!addr) return NULL;

    addr = addPageRange((uint64_t)addr, numOfPages * PAGE_SIZE, (uint64_t)addr, flags);
    return addr;
}

void freePages(void* address, size_t numOfPages) {
    for (size_t i = 0; i < numOfPages; i++) {
        freePage((uint64_t)address + i * PAGE_SIZE);
    }
}