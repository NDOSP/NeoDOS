#include "smp.h"
#include "bootinfo.h"
#include "madt.h"
#include "memory/paging.h"
#include "memory/vmm.h"
#include "memory/pmm.h"
#include "tss.h"
#include "interrupts/idt.h"
#include "interrupts/lapic.h"
#include "syscalls/syscalls.h"
#include "video.h"
#include "lock.h"
#include "string.h"
#include "panic.h"
#include "hal.h"
#include "serial.h"
#include "debug.h"

extern void loadGdt(uint64_t cpuId);
extern void idtLoad(void);
extern char trampoline_start[];
extern char trampoline_end[];

#define TRAMPOLINE_ADDR ((uint64_t)0x5000)
#define TRAMPOLINE_DATA_OFF 0x200

static volatile int apCpuCount = 0;
static volatile int cpuOnline[MAX_CPUS];
static void* apStacks[MAX_CPUS];

static uint64_t findLogicalCpuId(uint8_t apicId) {
    for (uint32_t i = 0; i < madtInfo.numCpus; i++) {
        if (madtInfo.cpus[i].apicId == apicId)
            return i;
    }
    return 0xFFFFFFFF;
}

void apEntry(void) {
    DEBUG_INFO("SMP: apEntry called");
    uint8_t lapicId = lapic_read(LAPIC_ID_REG) >> 24;
    uint64_t cpuId = findLogicalCpuId(lapicId);
    DEBUG_INFO("SMP: AP lapicId=%lX cpuId=%lX", lapicId, cpuId);
    if (cpuId == 0xFFFFFFFF) {
        DEBUG_ERROR("SMP: AP unknown CPU, halting");
        while (1) asm volatile("hlt");
    }

    loadGdt(cpuId);
    idtLoad();
    lapic_init();

    void* stack = apStacks[cpuId];
    DEBUG_INFO("SMP: AP stack=%lX", (uint64_t)stack);
    tss[cpuId].rsp0 = (uint64_t)stack + 0x3000;
    tss[cpuId].ist1 = (uint64_t)stack + 0x2000;
    tss[cpuId].ist2 = (uint64_t)stack + 0x1000;

    initSyscalls(cpuId, (void*)((uint64_t)stack + 0x3000));

    cpuOnline[cpuId] = 1;
    __sync_synchronize();

    DEBUG_INFO("SMP: CPU %lX online!", cpuId);

    drawOutput("CPU ", green);
    drawHex64(cpuId, green);
    drawOutput(" online!\n", green);

    asm volatile("sti");
    while (1) {
        asm volatile("hlt");
    }
}

void initAPs(void) {
    DEBUG_INFO("SMP: initAPs started");
    if (madtInfo.numCpus <= 1) {
        DEBUG_INFO("SMP: 1 CPU only");
        drawOutput("SMP: 1 CPU (BSP only)\n", white);
        return;
    }

    DEBUG_INFO("SMP: numCpus=%lX", madtInfo.numCpus);

    drawOutput("SMP: ", white);
    drawHex64(madtInfo.numCpus, white);
    drawOutput(" CPUs detected\n", white);

    size_t trampSize = (uint64_t)trampoline_end - (uint64_t)trampoline_start;
    DEBUG_INFO("SMP: trampoline size=%lX", trampSize);
    if (trampSize > TRAMPOLINE_DATA_OFF) {
        panic("Trampoline too large!");
    }

    addPage(TRAMPOLINE_ADDR, TRAMPOLINE_ADDR, PAGE_PRESENT | PAGE_WRITE);
    memcpy((void*)TRAMPOLINE_ADDR, trampoline_start, trampSize);
    DEBUG_INFO("SMP: trampoline copied");

    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    DEBUG_INFO("SMP: CR3=%lX", cr3);

    volatile uint32_t* pml4Field = (volatile uint32_t*)(TRAMPOLINE_ADDR + 0x200);
    volatile uint64_t* entryField = (volatile uint64_t*)(TRAMPOLINE_ADDR + 0x20C);

    *pml4Field = cr3 & 0xFFFFFFFF;
    *entryField = (uint64_t)&apEntry;
    DEBUG_INFO("SMP: trampoline data set");

    for (uint32_t i = 1; i < madtInfo.numCpus; i++) {
        cpuOnline[i] = 0;

        apStacks[i] = pmmAllocator(4);
        if (!apStacks[i]) {
            DEBUG_ERROR("SMP: Failed to alloc stack for CPU %lX", i);
            drawOutput("SMP: Failed to alloc stack for CPU ", red);
            drawHex64(i, red);
            drawOutput("\n", red);
            continue;
        }
        addPageRange((uint64_t)apStacks[i], 4 * PAGE_SIZE, (uint64_t)apStacks[i], PAGE_PRESENT | PAGE_WRITE);

        volatile uint64_t* stackField = (volatile uint64_t*)(TRAMPOLINE_ADDR + 0x204);
        *stackField = (uint64_t)apStacks[i] + 0x4000 - 0x10;

        uint8_t apicId = madtInfo.cpus[i].apicId;
        DEBUG_INFO("SMP: Starting CPU %lX apicId=%lX stack=%lX", i, apicId, *stackField);

        DEBUG_INFO("SMP: Sending INIT...");
        lapic_sendIPI(apicId, ICR_DELIVERY_INIT | ICR_ASSERT);
        for (volatile int d = 0; d < 5000000; d++) asm volatile("pause");

        DEBUG_INFO("SMP: Sending SIPI...");
        lapic_sendIPI(apicId, ICR_DELIVERY_SIPI | (TRAMPOLINE_ADDR >> 12));
        for (volatile int d = 0; d < 100000; d) asm volatile("pause");

        DEBUG_INFO("SMP: Sending SIPI again...");
        lapic_sendIPI(apicId, ICR_DELIVERY_SIPI | (TRAMPOLINE_ADDR >> 12));
        for (volatile int d = 0; d < 100000; d) asm volatile("pause");

        DEBUG_INFO("SMP: Waiting for CPU...");
        for (int timeout = 0; timeout < 50000000; timeout++) {
            if (cpuOnline[i]) break;
            asm volatile("pause");
        }

        if (cpuOnline[i]) {
            DEBUG_INFO("SMP: CPU %lX started OK", i);
            drawOutput("SMP: CPU ", white);
            drawHex64(i, white);
            drawOutput(" started\n", white);
        } else {
            DEBUG_ERROR("SMP: CPU %lX timeout!", i);
            drawOutput("SMP: CPU ", red);
            drawHex64(i, red);
            drawOutput(" timeout!\n", red);
        }
    }

    drawOutput("SMP: ", white);
    drawHex64(madtInfo.numCpus, white);
    drawOutput(" CPUs ready\n", white);
    DEBUG_INFO("SMP: initAPs done");
}
