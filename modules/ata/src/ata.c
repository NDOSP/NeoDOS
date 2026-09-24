#include "modstd.h"
#include <stdint.h>

REGISTER_MODULE("ata_pio");

#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_SEC_CNT   0x1F2
#define ATA_LBA_LO    0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HI    0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_CMD       0x1F7
#define ATA_ALT_STAT  0x3F6

#define SR_BSY  0x80
#define SR_DRDY 0x40
#define SR_DF   0x20
#define SR_DSC  0x10
#define SR_DRQ  0x08
#define SR_ERR  0x01

#define CMD_READ      0x20
#define CMD_IDENTIFY  0xEC

#define IPC_CMD_READ     1
#define IPC_CMD_GET_INFO 2
#define ATA_MASTER 0
#define ATA_SLAVE  1

static int drive_ok = 0;
static uint64_t drive_sectors = 0;
static unsigned long long buf_vaddr = 0;
static unsigned long long buf_phys = 0;

#define MAX_SHM_CACHE 8
static unsigned long long shm_handles[MAX_SHM_CACHE];
static unsigned long long shm_vaddrs[MAX_SHM_CACHE];
static unsigned long long shm_paddrs[MAX_SHM_CACHE];
static int shm_cache_count = 0;

static inline uint8_t inb(uint16_t port) {
    return (uint8_t)mod_syscall4(SYSCALL_MOD, MOD_PORT_IO, PORT_W(port, 8, PORT_IN), 0, 0);
}

static inline void outb(uint16_t port, uint8_t val) {
    mod_syscall4(SYSCALL_MOD, MOD_PORT_IO, PORT_W(port, 8, PORT_OUT), val, 0);
}

static int ata_wait(uint8_t mask, uint8_t expect) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(ATA_CMD);
        if ((s & mask) == expect) return 0;
        if (s & SR_ERR) return -1;
    }
    return -1;
}

static int ata_wait_bsy(void) {
    for (int i = 0; i < 1000000; i++) {
        if (!(inb(ATA_CMD) & SR_BSY)) return 0;
    }
    return -1;
}

static void ata_select(uint8_t drive) {
    outb(ATA_DRIVE, drive ? 0xF0 : 0xE0);
    for (volatile int i = 0; i < 50; i++);
}

static int ata_identify(uint8_t drive) {
    ata_select(drive);
    uint8_t st = inb(ATA_CMD);
    if (st == 0 || st == 0xFF) {
        debug_puts("ATA: no device, st=");
        debug_putu(st);
        debug_puts("\n");
        return -1;
    }
    if (ata_wait_bsy() != 0) return -1;

    outb(ATA_SEC_CNT, 0);
    outb(ATA_LBA_LO,  0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI,  0);
    outb(ATA_CMD, CMD_IDENTIFY);

    st = inb(ATA_CMD);
    if (st == 0) return -1;
    if (ata_wait_bsy() != 0) return -1;

    if (inb(ATA_LBA_MID) || inb(ATA_LBA_HI))
        return -1;

    if (ata_wait(SR_DRQ, SR_DRQ) != 0)
        return -1;

    mod_rep_insw(ATA_DATA, buf_phys, 256);

    uint8_t err = inb(ATA_ERROR);
    if (err) return -1;

    drive_sectors = *(uint32_t*)(buf_vaddr + 120);
    if (drive_sectors == 0)
        drive_sectors = *(uint64_t*)(buf_vaddr + 200);

    return 0;
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* args = (uint64_t*)msg;
    uint64_t cmd = args[0];
    uint64_t reply[8] = {0};

    switch (cmd) {
    case IPC_CMD_READ: {
        if (!drive_ok) { reply[0] = -1; break; }
        uint64_t lba = args[1];
        uint64_t count = args[2];
        uint64_t shm_handle = args[3];

        // Find or cache SHM mapping — avoid attach/detach per call (~30ms each)
        unsigned long long buf_v = 0, buf_p = 0;
        int found = 0;
        for (int i = 0; i < shm_cache_count; i++) {
            if (shm_handles[i] == shm_handle) {
                buf_v = shm_vaddrs[i];
                buf_p = shm_paddrs[i];
                found = 1;
                break;
            }
        }
        if (!found) {
            if (shm_cache_count >= MAX_SHM_CACHE) {
                reply[0] = -1;
                break;
            }
            buf_v = shm_attach(shm_handle);
            if (buf_v == (unsigned long long)-1) { reply[0] = -1; break; }
            buf_p = mod_phys_addr(buf_v);
            shm_handles[shm_cache_count] = shm_handle;
            shm_vaddrs[shm_cache_count] = buf_v;
            shm_paddrs[shm_cache_count] = buf_p;
            shm_cache_count++;
        }

        int ok = 1;
        for (uint64_t s = 0; s < count; s++) {
            if (ata_wait_bsy() != 0) { ok = 0; break; }
            ata_select(0);
            if (ata_wait_bsy() != 0) { ok = 0; break; }

            uint64_t cur = lba + s;
            outb(ATA_SEC_CNT, 1);
            outb(ATA_LBA_LO,  (uint8_t)(cur));
            outb(ATA_LBA_MID, (uint8_t)(cur >> 8));
            outb(ATA_LBA_HI,  (uint8_t)(cur >> 16));
            outb(ATA_DRIVE, 0xE0 | (uint8_t)((cur >> 24) & 0x0F));
            for (volatile int i = 0; i < 10; i++);

            outb(ATA_CMD, CMD_READ);

            if (ata_wait(SR_DRQ, SR_DRQ) != 0) { ok = 0; break; }

            mod_rep_insw(ATA_DATA, buf_p + s * 512, 256);

            uint8_t st = inb(ATA_CMD);
            if (st & SR_ERR) { ok = 0; break; }
        }

        if (!ok) { debug_puts("ATA: read fail lba="); debug_putu(lba); debug_puts(" cnt="); debug_putu(count); debug_puts("\n"); }
        reply[0] = ok ? 0 : -1;
        break;
    }

    case IPC_CMD_GET_INFO:
        reply[0] = 0;
        reply[1] = 512;
        reply[2] = drive_sectors;
        break;

    default:
        reply[0] = -1;
        break;
    }

    mod_send(sender, reply);
}

void init(void) {
    debug_puts("ATA: init\n");

    buf_vaddr = mod_syscall(SYSCALL_ALLOC_PAGES, 1, 0, 0);
    if (buf_vaddr == (unsigned long long)-1 || buf_vaddr == 0) {
        debug_puts("ATA: no mem for buffer\n");
        while (1) asm volatile("pause");
    }
    buf_phys = mod_phys_addr(buf_vaddr);

    if (ata_identify(ATA_MASTER) == 0) {
        drive_ok = 1;
        debug_puts("ATA: master detected, sectors=");
        debug_putu(drive_sectors);
        debug_puts("\n");
    } else if (ata_identify(ATA_SLAVE) == 0) {
        drive_ok = 1;
        debug_puts("ATA: slave detected, sectors=");
        debug_putu(drive_sectors);
        debug_puts("\n");
    } else {
        debug_puts("ATA: no drive\n");
    }

    debug_puts("ATA: ready\n");
}

void loop(void) {
    unsigned char msg[64];
    unsigned long long sender = mod_recv(msg);
    if (sender != (unsigned long long)-1)
        handle_ipc(msg, sender);
    else
        asm volatile("pause");
}
