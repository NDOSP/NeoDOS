#include "pmm.h"
#include "bootinfo.h"
#include "paging.h"
#include <stdbool.h>

static void markUsed(void* address, size_t numOfPages) {
    if (address == NULL || numOfPages == 0 || bInfo.memoryBitmapAddress == 0) return;

    uint64_t startPage = (uint64_t)address / PAGE_SIZE;
    uint8_t* bitmap = (uint8_t*)(size_t)bInfo.memoryBitmapAddress;

    for (size_t i = 0; i < numOfPages; i++) {
        uint64_t bit = startPage + i;
        uint64_t byteIndex = bit / 8;
        uint8_t bitIndex = bit % 8;
        bitmap[byteIndex] |= (1 << bitIndex);
    }
}

#include "video.h"

static void* findFreeAddress(size_t numOfPages) {
    if (numOfPages == 0 || bInfo.memoryBitmapAddress == 0) return NULL;

    uint8_t* bitmap = (uint8_t*)(size_t)bInfo.memoryBitmapAddress;
    uint64_t totalBits = bInfo.memoryBitmapPages * PAGE_SIZE * 8;

    size_t consecutive = 0;
    uint64_t startBit = 0;

    for (uint64_t bit = 0; bit < totalBits; bit++) {
        uint64_t byteIndex = bit / 8;
        uint8_t bitIndex = bit % 8;

        if ((bitmap[byteIndex] & (1 << bitIndex)) == 0) {
            if (consecutive == 0) startBit = bit;
            consecutive++;
            if (consecutive >= numOfPages) {
                return (void*)(startBit * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }

    return NULL;
}

void pmmFree(void* address, size_t numOfPages) {
    if (address == NULL || numOfPages == 0 || bInfo.memoryBitmapAddress == 0) return;

    uint64_t startPage = (uint64_t)address / PAGE_SIZE;
    uint8_t* bitmap = (uint8_t*)(size_t)bInfo.memoryBitmapAddress;

    for (size_t i = 0; i < numOfPages; i++) {
        uint64_t bit = startPage + i;
        uint64_t byteIndex = bit / 8;
        uint8_t bitIndex = bit % 8;
        bitmap[byteIndex] &= ~(1 << bitIndex);
    }
}

void* pmmAllocator(size_t numOfPages) {
    void* addr = findFreeAddress(numOfPages);
    markUsed(addr, numOfPages);
    return addr;
}
