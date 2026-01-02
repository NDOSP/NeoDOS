#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

void* pmmAllocator(size_t numOfPages);
void pmmFree(void* address, size_t numOfPages);

#endif // PMM_H