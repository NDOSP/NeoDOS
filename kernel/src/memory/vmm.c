#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "bootinfo.h"
#include <stdbool.h>

static inline void refreshTLB(void* vaddr) {
    asm volatile (
        "invlpg (%0)"
        :
        : "r"(vaddr)
        : "memory"
    );
}

static inline void reloadCR3(void) {
    uint64_t cr3;
    asm volatile (
        "mov %%cr3, %0\n"
        "mov %0, %%cr3\n"
        : "=r"(cr3)
        :
        : "memory"
    );
}

void* tempMap(void* new_page) {
    uint64_t* pml4 = PML4_VADDR;
    uint64_t* pdpt = (uint64_t*)bInfo.pageAllocatorTemporaryMemory;
    uint64_t* pd   = (uint64_t*)((uint8_t*)pdpt + PAGE_SIZE);
    uint64_t* pt   = (uint64_t*)((uint8_t*)pd   + PAGE_SIZE);

    pml4[TEMP_SLOT] = ((uint64_t)pdpt & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
    pdpt[0] = ((uint64_t)pd & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
    pd[0] = ((uint64_t)pt & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
    pt[0] = ((uint64_t)new_page & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;

    void* va = (void*)CANONICAL((uint64_t)TEMP_SLOT << 39);
    refreshTLB(va);
    return va;
}

void tempUnmap(void) {
    uint64_t* pml4 = PML4_VADDR;
    uint64_t* pdpt = (uint64_t*)bInfo.pageAllocatorTemporaryMemory;
    uint64_t* pd   = (uint64_t*)((uint8_t*)pdpt + PAGE_SIZE);
    uint64_t* pt   = (uint64_t*)((uint8_t*)pd   + PAGE_SIZE);

    pt[0] = 0;
    pd[0] = 0;
    pdpt[0] = 0;
    pml4[TEMP_SLOT] = 0;

    refreshTLB((void*)CANONICAL((uint64_t)TEMP_SLOT << 39));
}

#include "video.h"

void* addPage(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (vaddr % PAGE_SIZE != 0 || paddr % PAGE_SIZE != 0) return NULL;

    uint16_t pml4_i = PML4_IDX(vaddr);
    uint16_t pdpt_i = PDPT_IDX(vaddr);
    uint16_t pd_i   = PD_IDX(vaddr);
    uint16_t pt_i   = PT_IDX(vaddr);

    uint64_t* pml4 = PML4_VADDR;
    if (!(pml4[pml4_i] & PAGE_PRESENT)) {
        uint64_t new = (uint64_t)pmmAllocator(1);
        if (!new) return NULL;

        void* tmp = tempMap((void*)new);
        memset(tmp, 0, PAGE_SIZE);
        tempUnmap();

        pml4[pml4_i] = (new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
    }

    uint64_t* pdpt = PDPT_VADDR(pml4_i);
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        uint64_t new = (uint64_t)pmmAllocator(1);
        if (!new) return NULL;

        void* tmp = tempMap((void*)new);
        memset(tmp, 0, PAGE_SIZE);
        tempUnmap();

        pdpt[pdpt_i] = (new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
    }

    uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t new = (uint64_t)pmmAllocator(1);
        if (!new) return NULL;

        void* tmp = tempMap((void*)new);
        memset(tmp, 0, PAGE_SIZE);
        tempUnmap();

        pd[pd_i] = (new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;
    }

    uint64_t* pt = PT_VADDR(pml4_i, pdpt_i, pd_i);

    pt[pt_i] = (paddr & ENTRY_ADDR_MASK) | PAGE_PRESENT | flags; // #PF

    refreshTLB((void*)vaddr);
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
    refreshTLB((void*)vaddr);

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