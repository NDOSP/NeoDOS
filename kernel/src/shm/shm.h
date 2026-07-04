#ifndef SHM_H
#define SHM_H

#include <stdint.h>

#define MAX_SHM_REGIONS 64
#define GETBIT(x, idx) ((x >> idx) & 0b1)

typedef struct {
    uint64_t handle;
    uint64_t paddr;
    uint64_t pages;
    int      active;
    uint64_t creator;
    uint8_t access; // (creator) RWX | (guests) RWX  || Bit Order = (NNRRWWXX)
                    //                            N - Reserved R - Read W - Write X - Execute
                    //                            Bit Order = NNCGCGCG (C - Creator | G - Guest)
} ShmRegion;

#define CREATOR_READ 0b00100000
#define CREATOR_WRITE 0b00001000
#define CREATOR_EXECUTE 0b00000010
#define CREATOR_ALL_RIGHTS (CREATOR_EXECUTE | CREATOR_READ | CREATOR_WRITE)

#define GUEST_READ 0b00010000
#define GUEST_WRITE 0b00000100
#define GUEST_EXECUTE 0b00000001
#define GUEST_ALL_RIGHTS (GUEST_EXECUTE | GUEST_READ | GUEST_WRITE)

#define SHM_ALL_RIGHTS (GUEST_ALL_RIGHTS | CREATOR_ALL_RIGHTS)

void shm_init(void);
uint64_t shm_create(uint64_t pages, uint8_t rights);
int      shm_attach(uint64_t handle, uint64_t* out_vaddr);
int      shm_detach(uint64_t handle);

#endif
