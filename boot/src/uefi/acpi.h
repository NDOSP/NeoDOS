#ifndef ACPI_BOOT_H
#define ACPI_BOOT_H

#include <efi.h>
#include <efilib.h>

typedef struct {
    UINT8  Signature[8];  
    UINT8  Checksum;  
    UINT8  OemId[6];    
    UINT8  Revision;  
    UINT32 RsdtAddress;   
} __attribute__((packed)) RSDP;

EFI_STATUS findACPI(RSDP** rdsp);

#endif // ACPI_BOOT_H