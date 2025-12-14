#ifndef MEMORY_BOOT_H
#define MEMORY_BOOT_H

#include <efi.h>
#include <efilib.h>

#define KB(x) x*1024
#define MB(x) KB(x)*1024

#define PAGE_SIZE 4096

UINTN align_up(UINTN size, UINTN align);

#endif // MEMORY_BOOT_H