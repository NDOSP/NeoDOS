#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

void* addPage(uint64_t vaddr, uint64_t paddr, uint64_t flags);
void* addPageRange(uint64_t vaddr, size_t size, uint64_t paddr, uint64_t flags);

void freePages(void* address, size_t numOfPages);
void* allocatePages(size_t numOfPages, uint64_t flags);
uint64_t vmtoPm(uint64_t vaddr);
void makePageRangeUser(uint64_t vaddr, size_t size);

uint64_t vmm_create_user_pml4(void);
void vmm_map_in_cr3(uint64_t cr3, uint64_t vaddr, size_t size, uint64_t paddr, uint64_t flags);

void* tempMap(void* paddr);
void tempUnmap(void);

#endif // VMM_H