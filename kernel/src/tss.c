#include "tss.h"
#include "bootinfo.h"
#include "memory/paging.h"

typedef struct{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
}  __attribute__((packed)) GdtTssEntry;

extern uint64_t gdt64[];

TSS tss[255];

void initTss(void) {
    for (size_t cpuId = 0; cpuId < bInfo.stackCount; cpuId++) {
        tss[cpuId].reserved0 = 0;
        tss[cpuId].reserved1 = 0;
        tss[cpuId].reserved2 = 0;
        tss[cpuId].reserved3 = 0;

        tss[cpuId].rsp0 = 0xFFFFFFFFFFFFFFF0 - (cpuId * 3 + 1) * PAGE_SIZE;
        tss[cpuId].rsp1 = 0;
        tss[cpuId].rsp2 = 0;
        tss[cpuId].io_map_base = sizeof(TSS);

        tss[cpuId].ist1 = 0xFFFFFFFFFFFFFFF0 - (cpuId * 3 + 1) * 2 * PAGE_SIZE - PAGE_SIZE; // #DF
        tss[cpuId].ist2 = 0xFFFFFFFFFFFFFFF0 - (cpuId * 3 + 1) * 2 * PAGE_SIZE - 2 * PAGE_SIZE; // Other Interrupts
        tss[cpuId].ist3 = 0;
        tss[cpuId].ist4 = 0;
        tss[cpuId].ist5 = 0;
        tss[cpuId].ist6 = 0;
        tss[cpuId].ist7 = 0;

        GdtTssEntry* e = (GdtTssEntry*)&gdt64[5 + cpuId * 2];

        uint64_t base = (uint64_t)&tss[cpuId];
        uint32_t limit = sizeof(TSS) - 1;

        e->limit_low = limit & 0xFFFF;
        e->base_low = base & 0xFFFF;
        e->base_mid = (base >> 16) & 0xFF;
        e->access = 0x89; // Present, ring 0, type 9 (available 64-bit TSS)
        e->gran = (limit >> 16) & 0x0F;
        e->base_high = (base >> 24) & 0xFF;
        e->base_upper = base >> 32;
    }
}

void updateTssRsp0(uint64_t rsp0, size_t cpuId) {
    tss[cpuId].rsp0 = rsp0;
}