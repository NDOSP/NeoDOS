#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemId[6];
    char oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} __attribute__((packed)) AcpiSdtHeader;

typedef struct {
    char signature[8];      // "RSD PTR "
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t rsdtAddress;

    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    char reserved[3];
} __attribute__((packed)) Rsdp;

typedef struct {
    AcpiSdtHeader header;
    uint32_t tablePointers[];
} __attribute__((packed)) Rsdt;

typedef struct {
    AcpiSdtHeader header;
    uint64_t tablePointers[];
} __attribute__((packed)) Xsdt;

void acpiInit(void);

#endif // ACPI_H