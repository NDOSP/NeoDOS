#ifndef STRING_BOOT_H
#define STRING_BOOT_H

#include <efi.h>
#include <efilib.h>

CHAR8* UTF32toUTF8(const UINT32* utf32);
int UTF32Strcmp(const UINT32* s1, const UINT32* s2);

#endif // STRING_BOOT_H