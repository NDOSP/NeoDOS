#ifndef SHM_H
#define SHM_H

#include <stdint.h>

#define MAX_SHM_REGIONS 64

typedef struct {
    uint64_t handle;
    uint64_t paddr;
    uint64_t pages;
    int      active;
} ShmRegion;

void shm_init(void);
uint64_t shm_create(uint64_t pages);
int      shm_attach(uint64_t handle, uint64_t* out_vaddr);
int      shm_detach(uint64_t handle);

#endif
