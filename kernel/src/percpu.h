#ifndef PERCPU_H
#define PERCPU_H

#include <stdint.h>

typedef struct {
    void* kernelStack;
    void* userStack;
    uint64_t cpuId;
} PerCpuData;

#endif // PERCPU_H