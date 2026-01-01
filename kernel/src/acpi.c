#include "memory.h"
#include "bootinfo.h"
#include "madt.h"
#include "acpi.h"

static inline int acpiChecksum(void* table, size_t length) {
    uint8_t sum = 0;
    uint8_t* p = table;

    for (size_t i = 0; i < length; i++)
        sum += p[i];

    return sum == 0;
}

int validateRsdp(void) {
    Rsdp* rsdp = bInfo.rsdp;
    if (__builtin_memcmp(rsdp->signature, "RSD PTR ", 8) != 0) return 0;

    if (rsdp->revision < 2) return 0;

    if (!acpiChecksum(rsdp, rsdp->length)) return 0;

    return 1;
}

MadtInfo madtInfo = {0};

void parseMadt(AcpiSdtHeader* h) {
    Madt* madt = (Madt*)h;
    madtInfo.lapicAddress = madt->lapicAddress;
    madtInfo.flags = madt->flags;

    uint8_t* ptr = (uint8_t*)madt + sizeof(Madt);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t len  = ptr[1];

        switch (type) {
            case 0: {
                MadtLocalApicEntry* lapic = (MadtLocalApicEntry*)ptr;
                if ((lapic->flags & 1) && madtInfo.numCpus < MAX_CPUS) {
                    madtInfo.cpus[madtInfo.numCpus].acpiId = lapic->acpiProcessorId;
                    madtInfo.cpus[madtInfo.numCpus].apicId = lapic->apicId;
                    madtInfo.numCpus++;
                }
                break;
            }

            case 1: {
                MadtIoApicEntry* ioapic = (MadtIoApicEntry*)ptr;
                if (madtInfo.numIoApics < MAX_IOAPICS) {
                    madtInfo.ioApics[madtInfo.numIoApics].id = ioapic->ioApicId;
                    madtInfo.ioApics[madtInfo.numIoApics].address = ioapic->address;
                    madtInfo.ioApics[madtInfo.numIoApics].gsiBase = ioapic->globalSystemInterruptBase;
                    madtInfo.numIoApics++;
                }
                break;
            }

            case 2: {
                MadtIntOverrideEntry* io = (MadtIntOverrideEntry*)ptr;
                if (madtInfo.numOverrides < MAX_IRQ_OVERRIDES) {
                    madtInfo.overrides[madtInfo.numOverrides].bus = io->busSource;
                    madtInfo.overrides[madtInfo.numOverrides].irq = io->irqSource;
                    madtInfo.overrides[madtInfo.numOverrides].gsi = io->globalSystemInterrupt;
                    madtInfo.overrides[madtInfo.numOverrides].flags = io->flags;
                    madtInfo.numOverrides++;
                }
                break;
            }
        }

        ptr += len;
    }
}

void handleTable(AcpiSdtHeader* h) {
    if (!__builtin_memcmp(h->signature, "APIC", 4)) {
        parseMadt(h);
        return;
    }

    if (!__builtin_memcmp(h->signature, "FACP", 4)) {
        // parseFadt(h);
        return;
    }

    if (!__builtin_memcmp(h->signature, "HPET", 4)) {
        // parseHpet(h);
        return;
    }

    if (!__builtin_memcmp(h->signature, "MCFG", 4)) {
        // parseMcfg(h);
        return;
    }
}

void parseXSDT(AcpiSdtHeader* xsdt) {
    uint64_t* entries = (uint64_t*)((uint8_t*)xsdt + sizeof(AcpiSdtHeader));
    size_t entryCount = (xsdt->length - sizeof(AcpiSdtHeader)) / 8;

    for (size_t i = 0; i < entryCount; i++) {
        uint64_t tablePhys = entries[i];
        AcpiSdtHeader* tableHeader = addPageBootstrap(tablePhys & ENTRY_ADDR_MASK, tablePhys & ENTRY_ADDR_MASK, PAGE_PRESENT | PAGE_WRITE);

        size_t tableSize = PAGE_ALIGN_UP(tableHeader->length);
        addPageRangeBootstrap(tablePhys & ENTRY_ADDR_MASK, tableSize, tablePhys & ENTRY_ADDR_MASK, PAGE_PRESENT | PAGE_WRITE);

        AcpiSdtHeader* table = (AcpiSdtHeader*)tablePhys;
        if (!acpiChecksum(table, table->length)) continue;

        handleTable(table);
    }
}

void acpiInit(void) {
    if (!validateRsdp()) {
        // TODO: Panic
        asm volatile("hlt");
    }

    AcpiSdtHeader* xsdt = addPageBootstrap(bInfo.rsdp->xsdtAddress & ENTRY_ADDR_MASK, bInfo.rsdp->xsdtAddress  & ENTRY_ADDR_MASK, PAGE_PRESENT | PAGE_WRITE);
    if (!xsdt) {
        // TODO: Panic
        asm volatile("hlt");
    }

    addPageRangeBootstrap(bInfo.rsdp->xsdtAddress & ENTRY_ADDR_MASK, PAGE_ALIGN_UP(xsdt->length), bInfo.rsdp->xsdtAddress & ENTRY_ADDR_MASK, PAGE_PRESENT | PAGE_WRITE);
    parseXSDT((AcpiSdtHeader*)bInfo.rsdp->xsdtAddress);
}
