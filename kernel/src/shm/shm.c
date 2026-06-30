#include "shm.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "scheduler/scheduler.h"
#include "debug.h"

static ShmRegion regions[MAX_SHM_REGIONS];
static uint64_t next_handle = 1;
static int initialized = 0;

void shm_init(void) {
    for (int i = 0; i < MAX_SHM_REGIONS; i++)
        regions[i].active = 0;
    initialized = 1;
    DEBUG_INFO("SHM: shared memory subsystem initialized");
}

uint64_t shm_create(uint64_t pages) {
    if (!initialized) return -1ULL;
    if (pages == 0 || pages > 256) return -1ULL;

    void* phys = pmmAllocator(pages);
    if (!phys) return -1ULL;
    uint64_t paddr = (uint64_t)phys;

    for (uint64_t off = 0; off < pages * 4096ULL; off += 4096) {
        void* tmp = tempMap((void*)(paddr + off));
        memset(tmp, 0, 4096);
        tempUnmap();
    }

    int slot = -1;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (!regions[i].active) { slot = i; break; }
    }
    if (slot == -1) {
        pmmFree(phys, pages);
        return -1ULL;
    }

    uint64_t handle = next_handle++;
    regions[slot].handle = handle;
    regions[slot].paddr  = paddr;
    regions[slot].pages  = pages;
    regions[slot].active = 1;

    DEBUG_INFO("SHM: created handle=%lu paddr=%lX pages=%lu", handle, paddr, pages);
    return handle;
}

int shm_attach(uint64_t handle, uint64_t* out_vaddr) {
    if (!initialized) return -1;

    ShmRegion* reg = NULL;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (regions[i].active && regions[i].handle == handle) {
            reg = &regions[i];
            break;
        }
    }
    if (!reg) return -1;

    uint64_t size = reg->pages * 4096;
    Task* task = getCurrentTask();
    if (!task) return -1;

    uint64_t vaddr = task->vaddr_next;
    task->vaddr_next += size;

    if (task->cr3) {
        vmm_map_in_cr3(task->cr3, vaddr, size, reg->paddr,
                       PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    } else {
        addPageRange(vaddr, size, reg->paddr, PAGE_PRESENT | PAGE_WRITE);
    }

    if (out_vaddr) *out_vaddr = vaddr;
    DEBUG_INFO("SHM: attached handle=%lu at vaddr=%lX (pid=%lu)", handle, vaddr, task->id);
    return 0;
}

int shm_detach(uint64_t handle) {
    if (!initialized) return -1;

    ShmRegion* reg = NULL;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (regions[i].active && regions[i].handle == handle) {
            reg = &regions[i];
            break;
        }
    }
    if (!reg) return -1;

    DEBUG_INFO("SHM: detach handle=%lu", handle);
    return 0;
}
