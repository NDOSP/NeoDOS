#include "lapic.h"
#include "../madt.h"
#include "../memory/vmm.h"
#include "../memory/paging.h"

static uint64_t lapic_ptr = 0;

void lapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(lapic_ptr + reg) = value;
}

uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t*)(lapic_ptr + reg);
}

uint8_t lapic_id(void) {
    return (lapic_read(LAPIC_ID_REG) >> 24) & 0xFF;
}

void lapic_sendIPI(uint8_t apicId, uint32_t icrLow) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apicId << 24);
    lapic_write(LAPIC_ICR_LO, icrLow);
    while (lapic_read(LAPIC_ICR_LO) & (1 << 12));
}

void lapic_init(void) {
    if (lapic_ptr == 0) {
        lapic_ptr = (uint64_t)addPage(madtInfo.lapicAddress, madtInfo.lapicAddress, PAGE_PRESENT | PAGE_WRITE | PAGE_CACHE_DISABLE);
    }

    lapic_write(0xF0, lapic_read(0xF0) | 0x100 | 0xFF);
}