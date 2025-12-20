#include "memory.h"

UINTN align_up(UINTN size, UINTN align) {
    return (size + align - 1) & ~(align - 1);
}

EFI_STATUS initPage(PAGETABLEENTRY (**address)[512]) {
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS phys;

    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, PAGE_SIZE/PAGE_SIZE, &phys);
    if (EFI_ERROR(Status)) return Status;

    *address = (PAGETABLEENTRY (*)[512])phys;
    SetMem((VOID*)**address, PAGE_SIZE, 0x00);

    return EFI_SUCCESS;
}

EFI_STATUS addPage(PAGETABLEENTRY (*pml4)[512], UINT64 vaddr, UINT64 paddr, UINT64 flags) {
    EFI_STATUS Status;

    if (paddr % PAGE_SIZE != 0) return EFI_INVALID_PARAMETER;
    if (vaddr % PAGE_SIZE != 0) return EFI_INVALID_PARAMETER;

    PAGETABLEENTRY (*pdpt)[512];
    PAGETABLEENTRY (*pd)[512];
    PAGETABLEENTRY (*pt)[512];

    UINT64 pml4_i = PML4_IDX(vaddr);
    UINT64 pdpt_i = PDPT_IDX(vaddr);
    UINT64 pd_i   = PD_IDX(vaddr);
    UINT64 pt_i   = PT_IDX(vaddr);

    if (!((*pml4)[pml4_i] & ENTRY_PRESENT)) {
        Status = initPage(&pdpt);
        if (EFI_ERROR(Status)) return Status;

        (*pml4)[pml4_i] = ((UINT64)pdpt & ENTRY_ADDR_MASK) | ENTRY_PRESENT | ENTRY_RW;
    } else {
        pdpt = (PAGETABLEENTRY (*)[512])((*pml4)[pml4_i] & ENTRY_ADDR_MASK);
    }

    if (!((*pdpt)[pdpt_i] & ENTRY_PRESENT)) {
        Status = initPage(&pd);
        if (EFI_ERROR(Status)) return Status;

        (*pdpt)[pdpt_i] = ((UINT64)pd & ENTRY_ADDR_MASK) | ENTRY_PRESENT | ENTRY_RW;
    } else {
        pd = (PAGETABLEENTRY (*)[512])((*pdpt)[pdpt_i] & ENTRY_ADDR_MASK);
    }

    if (!((*pd)[pd_i] & ENTRY_PRESENT)) {
        Status = initPage(&pt);
        if (EFI_ERROR(Status)) return Status;

        (*pd)[pd_i] = ((UINT64)pt & ENTRY_ADDR_MASK) | ENTRY_PRESENT | ENTRY_RW;
    } else {
        pt = (PAGETABLEENTRY (*)[512])((*pd)[pd_i] & ENTRY_ADDR_MASK);
    }

    (*pt)[pt_i] = (paddr & ENTRY_ADDR_MASK) | flags | ENTRY_PRESENT;

    return EFI_SUCCESS;
}

EFI_STATUS getMemoryMap(MEMORY_MAP* map) {
    EFI_MEMORY_DESCRIPTOR* memMap;
    UINTN descriptorSize;
    UINT32 descriptorVersion;
    
    memMap = LibMemoryMap(&map->numberOfEntries, &map->mapKey, &descriptorSize, &descriptorVersion);
    map->descriptorSize = descriptorSize;
    map->descriptorVersion = descriptorVersion;

    UINTN outIndex = 0;
    for (UINTN i = 0; i < map->numberOfEntries; i++) {
        EFI_MEMORY_DESCRIPTOR desc = memMap[i];
        UINT32 type = desc.Type;
        
        if (type == EfiBootServicesCode || type == EfiBootServicesData) {
            type = EfiConventionalMemory;
        }

        UINT64 start = desc.PhysicalStart;
        UINT64 size = desc.NumberOfPages * 4096;

        if (outIndex > 0 && map->entries[outIndex - 1].type == EfiConventionalMemory && type == EfiConventionalMemory && map->entries[outIndex - 1].address + map->entries[outIndex - 1].size == start) {
            map->entries[outIndex - 1].size += size;
        } else {
            map->entries[outIndex].address = start;
            map->entries[outIndex].size = size;
            map->entries[outIndex].type = type;
            outIndex++;
        }
    }

    map->numberOfEntries = outIndex;
    return EFI_SUCCESS;
}
