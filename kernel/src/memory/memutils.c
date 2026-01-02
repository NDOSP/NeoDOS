#include "memutils.h"

void* memcpy(void* dest, const void* src, uint32_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* s, int c, uint32_t n) {
    uint8_t *p = s;
    for (uint32_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}