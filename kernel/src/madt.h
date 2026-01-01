#ifndef MADT_H
#define MADT_H

#include <stdint.h>

typedef struct {
    AcpiSdtHeader header;
    uint32_t lapicAddress;
    uint32_t flags;
} __attribute__((packed)) Madt;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) MadtEntry;

typedef struct __attribute__((packed)) {
    uint8_t type;       // 0
    uint8_t length;     // 8
    uint8_t acpiProcessorId;
    uint8_t apicId;
    uint32_t flags;     // bit0=enabled
} MadtLocalApicEntry;

typedef struct __attribute__((packed)) {
    uint8_t type;       // 1
    uint8_t length;     // 12
    uint8_t ioApicId;
    uint8_t reserved;
    uint32_t address;   // MMIO
    uint32_t globalSystemInterruptBase;
} MadtIoApicEntry;

typedef struct __attribute__((packed)) {
    uint8_t type;       // 2
    uint8_t length;     // 10
    uint8_t busSource;
    uint8_t irqSource;
    uint32_t globalSystemInterrupt;
    uint16_t flags;
} MadtIntOverrideEntry;

#define MAX_CPUS   16
#define MAX_IOAPICS 4
#define MAX_IRQ_OVERRIDES 16

typedef struct {
    uint8_t acpiId;
    uint8_t apicId;
} LocalApic;

typedef struct {
    uint8_t id;
    uint32_t address;
    uint32_t gsiBase;
} IoApic;

typedef struct {
    uint8_t bus;
    uint8_t irq;
    uint32_t gsi;
    uint16_t flags;
} IntOverride;

typedef struct {
    uint32_t lapicAddress;
    uint32_t flags;

    uint32_t numCpus;
    LocalApic cpus[MAX_CPUS];

    uint32_t numIoApics;
    IoApic ioApics[MAX_IOAPICS];

    uint32_t numOverrides;
    IntOverride overrides[MAX_IRQ_OVERRIDES];
} MadtInfo;

extern MadtInfo madtInfo;

#endif // MADT_H