#include "string.h"

CHAR8* UTF32toUTF8(const UINT32* utf32) {
    if (!utf32) return NULL;
    
    UINTN len = 0;
    const UINT32* p = utf32;
    while (*p && *p <= 0x7F) {
        len++;
        p++;
    }
    
    if (*p > 0x7F) {
        return NULL;
    }
    
    CHAR8* result = (CHAR8*)AllocatePool(len + 1);
    if (!result) return NULL;
    
    for (UINTN i = 0; i < len; i++) {
        result[i] = (CHAR8)(utf32[i] & 0xFF);
    }
    result[len] = '\0';
    
    return result;
}

int UTF32Strcmp(const UINT32* s1, const UINT32* s2) {
    if (!s1 || !s2) {
        return -1;
    }
    
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    
    return (int)(*s1 - *s2);
}
