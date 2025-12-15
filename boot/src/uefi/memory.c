#include "memory.h"

UINTN align_up(UINTN size, UINTN align) {
    return (size + align - 1) & ~(align - 1);
}

EFI_STATUS initPage(UINT64** address) {
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS* page;

    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, (UINTN)1, (VOID**)&page);
    if (EFI_ERROR(Status)) return Status;

    SetMem((VOID*)page, 4096, 0);
    *address = (UINT64*)page;

    return EFI_SUCCESS;
}


EFI_STATUS addPage(UINT64* pml4, UINT64 vaddr, UINT64 paddr, UINT64 flags) {
    EFI_STATUS Status;
    UINT64 pml4_index = (vaddr >> 39) & 0x1FF;
    UINT64 pdpt_index = (vaddr >> 30) & 0x1FF;
    UINT64 pd_index   = (vaddr >> 21) & 0x1FF;
    UINT64 pt_index   = (vaddr >> 12) & 0x1FF;

    UINT64* pdpt;
    UINT64* pd;
    UINT64* pt;
    UINT64* new_table;

    if (!(pml4[pml4_index] & PAGE_PRESENT)) {
        Status = initPage(&new_table);
        if (EFI_ERROR(Status)) return Status;
        pml4[pml4_index] = (UINT64)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }
    pdpt = (UINT64*)(pml4[pml4_index] & ~0xFFF);

    if (!(pdpt[pdpt_index] & PAGE_PRESENT)) {
        Status = initPage(&new_table);
        if (EFI_ERROR(Status)) return Status;
        pdpt[pdpt_index] = (UINT64)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }
    pd = (UINT64*)(pdpt[pdpt_index] & ~0xFFF);

    if (!(pd[pd_index] & PAGE_PRESENT)) {
        Status = initPage(&new_table);
        if (EFI_ERROR(Status)) return Status;
        pd[pd_index] = (UINT64)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }
    pt = (UINT64*)(pd[pd_index] & ~0xFFF);

    pt[pt_index] = paddr | flags;

    return EFI_SUCCESS;
}

