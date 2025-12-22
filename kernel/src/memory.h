#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

void* memset(void* s, int c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);

#endif // MEMORY_H