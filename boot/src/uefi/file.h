#ifndef FILE_BOOT_H
#define FILE_BOOT_H

#include <efi.h>
#include <efilib.h>

EFI_STATUS loadFile(IN CHAR16* path, OUT VOID** data, OUT UINTN* fileSize);

#endif // FILE_BOOT_H