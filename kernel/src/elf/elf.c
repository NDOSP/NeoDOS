#include "elf.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "debug.h"

int elfLoad(void* data, uint64_t* entry, uint64_t* stackTop) {
    Elf64Header* hdr = (Elf64Header*)data;
    if (*(uint32_t*)hdr->e_ident != ELF_MAGIC) {
        DEBUG_ERROR("ELF: bad magic");
        return -1;
    }

    *entry = hdr->e_entry;
    DEBUG_INFO("ELF: entry=%lX, phoff=%lX, phnum=%u", hdr->e_entry, hdr->e_phoff, hdr->e_phnum);

    Elf64Phdr* phdr = (Elf64Phdr*)((uint8_t*)data + hdr->e_phoff);

    for (uint16_t i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint64_t vaddr = phdr[i].p_vaddr;
        uint64_t memsz = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;
        uint64_t flags = phdr[i].p_flags;

        uint64_t pageStart = PAGE_ALIGN_DOWN(vaddr);
        uint64_t pageEnd = PAGE_ALIGN_UP(vaddr + memsz);
        uint64_t numPages = (pageEnd - pageStart) / PAGE_SIZE;

        DEBUG_INFO("ELF: LOAD vaddr=%lX filesz=%lX memsz=%lX pages=%lu", vaddr, filesz, memsz, numPages);

        uint64_t paddr = (uint64_t)pmmAllocator(numPages);
        if (!paddr) {
            DEBUG_ERROR("ELF: OOM for %lu pages", numPages);
            return -1;
        }

        uint64_t mmapFlags = PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
        if (!(flags & 0x1)) mmapFlags |= PAGE_EXEC_DISABLE;
        if (!(flags & 0x2)) mmapFlags &= ~PAGE_WRITE;
        addPageRange(pageStart, numPages * PAGE_SIZE, paddr, mmapFlags);

        void* mapped = (void*)pageStart;
        uint64_t fileCopy = filesz < memsz ? filesz : memsz;
        memcpy((void*)((uint64_t)mapped + (vaddr - pageStart)), (uint8_t*)data + offset, fileCopy);

        uint64_t bssStart = vaddr + filesz;
        uint64_t bssEnd = vaddr + memsz;
        if (bssEnd > bssStart) {
            memset((void*)((uint64_t)mapped + (bssStart - pageStart)), 0, bssEnd - bssStart);
        }

        DEBUG_INFO("ELF: segment mapped at %lX (%lu pages)", pageStart, numPages);
    }

    uint64_t stackAddr = USER_STACK_VADDR;
    void* stackPhys = pmmAllocator(4);
    if (!stackPhys) {
        DEBUG_ERROR("ELF: OOM for stack");
        return -1;
    }
    addPageRange(stackAddr, USER_STACK_SIZE, (uint64_t)stackPhys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    *stackTop = stackAddr + USER_STACK_SIZE;

    DEBUG_INFO("ELF: load OK, entry=%lX stack=%lX", *entry, *stackTop);
    return 0;
}
