#ifndef ACPI_BOOT_H
#define ACPI_BOOT_H

#include <efi.h>
#include <efilib.h>

typedef struct {
    CHAR8 Signature[8];  
    UINT8 Checksum;
    CHAR8 OemId[6];
    UINT8 Revision;    
    UINT32 RsdtAddress;
    UINT32 Length;
    UINT64 XsdtAddress;
    UINT8 ExtendedChecksum;
    UINT8 Reserved[3];
} RSDP;


EFI_STATUS findACPI(RSDP** rdsp);

#endif // ACPI_BOOT_H