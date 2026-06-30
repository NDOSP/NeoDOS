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

uint64_t vmm_create_user_pml4(void) {
    uint64_t new_pml4_phys = (uint64_t)pmmAllocator(1);
    if (!new_pml4_phys) return 0;

    void* new_pml4_vaddr = tempMap((void*)new_pml4_phys);
    memset(new_pml4_vaddr, 0, PAGE_SIZE);

    uint64_t* current_pml4 = PML4_VADDR;

    // Shallow-copy kernel-range entries (256-511) — shared with kernel
    for (int i = 256; i < 512; i++)
        ((uint64_t*)new_pml4_vaddr)[i] = current_pml4[i];

    // Deep-copy user-range entries (0-255) so each task has private page tables
    for (int i = 0; i < 256; i++) {
        if (!(current_pml4[i] & PAGE_PRESENT)) continue;

        uint64_t pdpt_flags = current_pml4[i] & ~ENTRY_ADDR_MASK;

        uint64_t new_pdpt_phys = (uint64_t)pmmAllocator(1);
        if (!new_pdpt_phys) continue;

        tempUnmap();
        void* new_pdpt_vaddr = tempMap((void*)new_pdpt_phys);
        memset(new_pdpt_vaddr, 0, PAGE_SIZE);

        uint64_t* kernel_pdpt = PDPT_VADDR(i);
        for (int j = 0; j < 512; j++) {
            if (!(kernel_pdpt[j] & PAGE_PRESENT)) continue;

            if (kernel_pdpt[j] & PAGE_PAGE_SIZE) {
                ((uint64_t*)new_pdpt_vaddr)[j] = kernel_pdpt[j];
                continue;
            }

            uint64_t pd_flags = kernel_pdpt[j] & ~ENTRY_ADDR_MASK;

            uint64_t new_pd_phys = (uint64_t)pmmAllocator(1);
            if (!new_pd_phys) continue;

            tempUnmap();
            void* new_pd_vaddr = tempMap((void*)new_pd_phys);
            memset(new_pd_vaddr, 0, PAGE_SIZE);

            uint64_t* kernel_pd = PD_VADDR(i, j);
            for (int k = 0; k < 512; k++) {
                if (!(kernel_pd[k] & PAGE_PRESENT)) continue;

                if (kernel_pd[k] & PAGE_PAGE_SIZE) {
                    ((uint64_t*)new_pd_vaddr)[k] = kernel_pd[k];
                    continue;
                }

                uint64_t pt_flags = kernel_pd[k] & ~ENTRY_ADDR_MASK;

                uint64_t new_pt_phys = (uint64_t)pmmAllocator(1);
                if (!new_pt_phys) continue;

                tempUnmap();
                void* new_pt_vaddr = tempMap((void*)new_pt_phys);
                uint64_t* kernel_pt = PT_VADDR(i, j, k);
                memcpy(new_pt_vaddr, kernel_pt, PAGE_SIZE);
                tempUnmap();

                new_pd_vaddr = tempMap((void*)new_pd_phys);
                ((uint64_t*)new_pd_vaddr)[k] = (new_pt_phys & ENTRY_ADDR_MASK) | pt_flags;
            }

            tempUnmap();
            new_pdpt_vaddr = tempMap((void*)new_pdpt_phys);
            ((uint64_t*)new_pdpt_vaddr)[j] = (new_pd_phys & ENTRY_ADDR_MASK) | pd_flags;
        }

        tempUnmap();
        new_pml4_vaddr = tempMap((void*)new_pml4_phys);
        ((uint64_t*)new_pml4_vaddr)[i] = (new_pdpt_phys & ENTRY_ADDR_MASK) | pdpt_flags;
    }

    ((uint64_t*)new_pml4_vaddr)[RECURSIVE_SLOT] = (new_pml4_phys & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;

    tempUnmap();
    return new_pml4_phys;
}

void vmm_map_in_cr3(uint64_t cr3, uint64_t vaddr, size_t size, uint64_t paddr, uint64_t flags) {
    uint64_t old_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(old_cr3));

    if (cr3 && cr3 != old_cr3) {
        asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    }

    addPageRange(vaddr, size, paddr, flags);

    if (cr3 && cr3 != old_cr3) {
        asm volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
    }
}

void* addPage(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (vaddr % PAGE_SIZE != 0 || paddr % PAGE_SIZE != 0) return NULL;

    uint64_t userflag = flags & PAGE_USER ? PAGE_USER : 0;

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

        pml4[pml4_i] = (new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | userflag;
    }

    if (!(pml4[pml4_i] & PAGE_USER) && userflag) {
        pml4[pml4_i] |= PAGE_USER;
    }

    uint64_t* pdpt = PDPT_VADDR(pml4_i);
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        uint64_t new = (uint64_t)pmmAllocator(1);
        if (!new) return NULL;

        void* tmp = tempMap((void*)new);
        memset(tmp, 0, PAGE_SIZE);
        tempUnmap();

        pdpt[pdpt_i] = (new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | userflag;
    }

    if (!(pdpt[pdpt_i] & PAGE_USER) && userflag) {
        pdpt[pdpt_i] |= PAGE_USER;
    }

    uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t new = (uint64_t)pmmAllocator(1);
        if (!new) return NULL;

        void* tmp = tempMap((void*)new);
        memset(tmp, 0, PAGE_SIZE);
        tempUnmap();

        pd[pd_i] = (new & ENTRY_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | userflag;
    }

    if (!(pd[pd_i] & PAGE_USER) && userflag) {
        pd[pd_i] |= PAGE_USER;
    }

    uint64_t* pt = PT_VADDR(pml4_i, pdpt_i, pd_i);

    pt[pt_i] = (paddr & ENTRY_ADDR_MASK) | PAGE_PRESENT | flags;

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

void makePageRangeUser(uint64_t vaddr, size_t size) {
    for (size_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t addr = PAGE_ALIGN_DOWN(vaddr) + off;
        uint16_t pml4_i = PML4_IDX(addr);
        uint16_t pdpt_i = PDPT_IDX(addr);
        uint16_t pd_i   = PD_IDX(addr);
        uint16_t pt_i   = PT_IDX(addr);

        uint64_t* pml4 = PML4_VADDR;
        if (!(pml4[pml4_i] & PAGE_PRESENT)) continue;

        uint64_t* pdpt = PDPT_VADDR(pml4_i);
        if (!(pdpt[pdpt_i] & PAGE_PRESENT)) continue;

        uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
        if (!(pd[pd_i] & PAGE_PRESENT)) continue;

        uint64_t* pt = PT_VADDR(pml4_i, pdpt_i, pd_i);
        pt[pt_i] |= PAGE_USER;
        refreshTLB((void*)addr);
    }

    // Also set USER in higher-level tables so future walks propagate correctly
    for (size_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t addr = PAGE_ALIGN_DOWN(vaddr) + off;
        uint16_t pml4_i = PML4_IDX(addr);
        uint16_t pdpt_i = PDPT_IDX(addr);
        uint16_t pd_i   = PD_IDX(addr);

        uint64_t* pml4 = PML4_VADDR;
        uint64_t* pdpt = PDPT_VADDR(pml4_i);
        uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);

        if (pd[pd_i] & PAGE_PRESENT) pd[pd_i] |= PAGE_USER;
        if (pdpt[pdpt_i] & PAGE_PRESENT) pdpt[pdpt_i] |= PAGE_USER;
        if (pml4[pml4_i] & PAGE_PRESENT) pml4[pml4_i] |= PAGE_USER;
    }
}

uint64_t vmtoPm(uint64_t vaddr) {
    uint16_t pml4_i = PML4_IDX(vaddr);
    uint16_t pdpt_i = PDPT_IDX(vaddr);
    uint16_t pd_i   = PD_IDX(vaddr);
    uint16_t pt_i   = PT_IDX(vaddr);

    uint64_t* pml4 = PML4_VADDR;
    if (!(pml4[pml4_i] & PAGE_PRESENT)) return 0;

    uint64_t* pdpt = PDPT_VADDR(pml4_i);
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return 0;
    
    if (pdpt[pdpt_i] & PAGE_PAGE_SIZE) {
        return (pdpt[pdpt_i] & 0xFFFFFC0000000ULL) + (vaddr & 0x3FFFFFFF);
    }

    uint64_t* pd = PD_VADDR(pml4_i, pdpt_i);
    if (!(pd[pd_i] & PAGE_PRESENT)) return 0;

    if (pd[pd_i] & PAGE_PAGE_SIZE) {
        return (pd[pd_i] & 0xFFFFFFFE00000ULL) + (vaddr & 0x1FFFFF);
    }

    uint64_t* pt = PT_VADDR(pml4_i, pdpt_i, pd_i);
    if (!(pt[pt_i] & PAGE_PRESENT)) return 0;

    return (pt[pt_i] & ENTRY_ADDR_MASK) + (vaddr & 0xFFF);
}
