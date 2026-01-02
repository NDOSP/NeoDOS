#ifndef MEMUTILS_H
#define MEMUTILS_H

#include <stdint.h>

#define KB(x) ((x) * 1024ULL)
#define MB(x) (KB(x) * 1024ULL)
#define GB(x) (MB(x) * 1024ULL)

#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(size, align) ((size) & ~((align) - 1))

void* memset(void* s, int c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);

#endif // MEMUTILS_H