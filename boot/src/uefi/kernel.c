#include "kernel.h"
#include "file.h"
#include "video.h"

EFI_STATUS loadElf(CHAR16* path, KERNEL_INFO* info) {
    UINT8* fileData;
    UINTN fileSize;
    EFI_STATUS Status = loadFile(path, (VOID**)&fileData, &fileSize);
    if (EFI_ERROR(Status)) return Status;

    if (fileData[0] != ELFMAG0 || fileData[1] != ELFMAG1 || fileData[2] != ELFMAG2 || fileData[3] != ELFMAG3) {
        Print(L"ERROR: (kernel) Invalid ELF Signature\n");
        return EFI_UNSUPPORTED;
    }

    Elf64_Ehdr ehdr;
    CopyMem(&ehdr, fileData, sizeof(Elf64_Ehdr));

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        Print(L"ERROR: (kernel) Unsuported class: 0x%01X\n", ehdr.e_ident[EI_CLASS]);
        return EFI_UNSUPPORTED;
    }

    Print(L"INFO: (kernel) ELF class ok=0x%01X\n", ehdr.e_ident[EI_CLASS]);

    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        Print(L"ERROR: (kernel) Unsuported endianness: 0x%01X\n", ehdr.e_ident[EI_DATA]);
        return EFI_UNSUPPORTED;
    }

    Print(L"INFO: (kernel) ELF endianness ok=0x%01X\n", ehdr.e_ident[EI_DATA]);

    if (ehdr.e_machine != EM_X86_64) {
        Print(L"ERROR: (kernel) Unsuported machine: 0x%02X\n", ehdr.e_machine);
        return EFI_UNSUPPORTED;
    }

    Print(L"INFO: (kernel) ELF machine ok=0x%02X\n", ehdr.e_machine);

    if (ehdr.e_type != ET_EXEC) {
        Print(L"ERROR: (kernel) Unsuported type: 0x%02X\n", ehdr.e_type);
        return EFI_UNSUPPORTED;
    }

    Print(L"INFO: (kernel) ELF type ok=0x%02X\n", ehdr.e_type);

    UINT64 phOffset = ehdr.e_phoff;
    UINTN phSize = ehdr.e_phnum * ehdr.e_phentsize;

    if (phOffset + phSize > fileSize) {
        Print(L"ERROR: (kernel) ELF program headers exceed kernel content bounds!\n");
        return EFI_END_OF_FILE;
    }

    Elf64_Phdr *pheaders = (Elf64_Phdr *)(fileData + ehdr.e_phoff);
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr = pheaders[i];
        if (phdr.p_filesz == 0 || phdr.p_memsz == 0) {
            Print(L"WARNING: (kernel) Empty program header(p_type=0x%04X; p_offset=0x%08X; p_filesz=0x%08X; p_memsz=0x%08X), skipping...\n", phdr.p_type, phdr.p_offset, phdr.p_filesz, phdr.p_memsz);
            continue;
        }

        if (phdr.p_type != PT_LOAD) {
            Print(L"INFO: (kernel) Skipping program header: p_type=0x%04X; p_offset=0x%08X; p_filesz=0x%08X; p_memsz=0x%08X\n", phdr.p_type, phdr.p_offset, phdr.p_filesz, phdr.p_memsz);
            continue;
        }

        Print(L"INFO: (kernel) Loading program header: p_type=0x%04X; p_offset=0x%08X; p_filesz=0x%08X; p_memsz=0x%08X\n", phdr.p_type, phdr.p_offset, phdr.p_filesz, phdr.p_memsz);

        UINTN segmentFileSize = phdr.p_filesz;
        UINTN memSize = phdr.p_memsz;
        Print(L"INFO: (kernel) Kernel segment at 0x%lX size=%ld bytes\n", phdr.p_offset, phdr.p_memsz);

        if (memSize > MB(64)) {
            Print(L"WARNING: (kernel) Kernel segment at 0x%lX might be too large\n", phdr.p_offset);
        }

        UINT64 pageOffset = phdr.p_vaddr & (PAGE_SIZE - 1);
        UINT64 totalSize  = align_up(pageOffset + phdr.p_memsz, PAGE_SIZE);
        UINTN pagesToAllocate = totalSize / PAGE_SIZE;
        UINT8* loadBuffer;
        Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pagesToAllocate, (VOID*)&loadBuffer);
        if (EFI_ERROR(Status)) return Status;
        Print(L"INFO: (kernel) Allocated %d pages for segment(p_vaddr: 0x%lX) at 0x%lX\n", pagesToAllocate, phdr.p_vaddr, loadBuffer);

        CopyMem(loadBuffer + pageOffset, fileData + phdr.p_offset, phdr.p_filesz);
        Print(L"INFO: (kernel) Loaded segment at 0x%lX, size %ld bytes from file offset 0x%lX\n", loadBuffer, segmentFileSize, phdr.p_offset);

        UINTN bssSize = memSize - segmentFileSize;
        if (bssSize > 0) {
            SetMem(loadBuffer + pageOffset + phdr.p_filesz, phdr.p_memsz - phdr.p_filesz, 0);
            Print(L"INFO: (kernel) Cleared .bss [%ld..%ld) = %ld bytes\n", segmentFileSize, memSize, bssSize);
        }

        info->segmentMapping[info->segmentCount].vaddr = phdr.p_vaddr & ~(PAGE_SIZE - 1);
        info->segmentMapping[info->segmentCount].paddr = (UINT64)loadBuffer;
        info->segmentMapping[info->segmentCount].size  = totalSize;
        info->segmentMapping[info->segmentCount].flags = phdr.p_flags;
        info->segmentCount++;

        if (phdr.p_flags == PF_R) {
            Print(L"INFO: (kernel) Found .bootinfo Program Segment at (paddr: 0x%lX, vaddr: 0x%lX)\n", (UINT64)loadBuffer, phdr.p_vaddr);
            info->bootInfo.flags = phdr.p_flags;
            info->bootInfo.vaddr = phdr.p_vaddr & ~(PAGE_SIZE - 1);
            info->bootInfo.paddr = (UINT64)loadBuffer;
            info->bootInfo.size = memSize;
        }

        Print(L"INFO: (kernel) Loaded Program Segment:\n\tp_paddr: 0x%lX\n\tp_vaddr: 0x%lX\n\tp_memsz: %ld bytes\n", (UINT64)loadBuffer, phdr.p_vaddr, memSize);
    }

    info->entryPoint = ehdr.e_entry;
    return EFI_SUCCESS;
}

EFI_STATUS map_core_stacks(PAGETABLEENTRY (*pml4)[512], UINT64 maxCpu) {
    EFI_STATUS Status;
    UINT64 coreStackVaddr = (UINT64)(-(INT64)PAGE_SIZE);

    for (UINT64 i = 0; i < (maxCpu * 3) + 1; i++) {
        void* coreStack;
        Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, &coreStack);
        if (EFI_ERROR(Status)) return Status;

        Status = addPage(pml4, coreStackVaddr - i * 2 * PAGE_SIZE, (UINT64)coreStack, ENTRY_PRESENT | ENTRY_RW | ENTRY_EXEC_DISABLE);
        if (EFI_ERROR(Status)) return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS mapKernelSpace(PAGETABLEENTRY (*pml4)[512], KERNEL_INFO* kInfo, VIDEO_FRAMEBUFFER* fb, UINT64 maxCpu, FONT_INFO* font) {
    EFI_STATUS Status;
    Status = map_core_stacks(pml4, maxCpu);
    if (EFI_ERROR(Status)) return Status;

    for (UINTN i = 0; i < kInfo->segmentCount; i++) {
        MAPPING_INFO* mapping = &kInfo->segmentMapping[i];
        UINT64 flags = 0;
        if (!(mapping->flags & PF_X)) flags |= ENTRY_EXEC_DISABLE;
        if (mapping->flags & PF_W) flags |= ENTRY_RW;

        for (UINT64 offset = 0; offset < mapping->size; offset += PAGE_SIZE) {
            Status = addPage(pml4, mapping->vaddr + offset, mapping->paddr + offset, ENTRY_PRESENT | flags);
            if (EFI_ERROR(Status)) return Status;
        }
    }

    if (fb) {
        UINT64 fbSize = fb->fbSize * 4;
        for (UINT64 offset = 0; offset < fbSize; offset += PAGE_SIZE) {
            Status = addPage(pml4, (UINT64)(fb->fbPtr + offset), (UINT64)(fb->fbPtr + offset), ENTRY_PRESENT | ENTRY_RW | ENTRY_CACHE_DISABLE | ENTRY_EXEC_DISABLE); 
            if (EFI_ERROR(Status)) return Status;
        }
    }

    if (font) {
        UINT64 fontStart = (UINT64)font;
        UINT64 fontSizePages = (sizeof(FONT_INFO) + font->glyphCount * sizeof(FONT_GLYPH) + font->glyphCount * font->bytesPerGlyph + PAGE_SIZE - 1) / PAGE_SIZE;

        for (UINT64 offset = 0; offset < fontSizePages * PAGE_SIZE; offset += PAGE_SIZE) {
            Status = addPage(pml4, fontStart + offset, fontStart + offset, ENTRY_PRESENT | ENTRY_EXEC_DISABLE);
            if (EFI_ERROR(Status)) return Status;
        }
    }

    return EFI_SUCCESS;
}
