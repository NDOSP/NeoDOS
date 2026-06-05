#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

void* addPage(uint64_t vaddr, uint64_t paddr, uint64_t flags);
void* addPageRange(uint64_t vaddr, size_t size, uint64_t paddr, uint64_t flags);

void freePages(void* address, size_t numOfPages);
void* allocatePages(size_t numOfPages, uint64_t flags);
uint64_t vmtoPm(uint64_t vaddr);

#endif // VMM_H