#include "modlib.h"
#include <stdint.h>

MODINFO("gpt");

typedef struct {
    uint64_t pid;
    char name[64];
} ModEntry;

#define GPT_CMD_GET_COUNT  1
#define GPT_CMD_GET_PART   2
#define GPT_CMD_FIND_ESP   3

#define ATA_CMD_READ 1
#define ATA_SECTORS  512

static uint64_t ata_pid = 0;
static unsigned long long shm_handle = 0;
static unsigned long long shm_vaddr = 0;

static int part_count = 0;
static uint64_t part_start[64];
static uint64_t part_end[64];
static uint8_t  part_guid[64][16];

static const uint8_t esp_type_guid[16] = {
    0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
    0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

static int guid_cmp(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 16; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static uint64_t find_module(const char* name) {
    unsigned long long buf = mod_syscall(SYSCALL_ALLOC_PAGES, 2, 0, 0);
    if (buf == 0 || buf == (unsigned long long)-1) return 0;

    int cnt = mod_list((void*)buf, 64);
    ModEntry* e = (ModEntry*)buf;
    for (int i = 0; i < cnt; i++) {
        int match = 1;
        for (int j = 0; name[j]; j++) {
            if (e[i].name[j] != name[j]) { match = 0; break; }
        }
        if (match && e[i].pid) return e[i].pid;
    }
    return 0;
}

static int ata_read(uint64_t lba, uint32_t count) {
    uint64_t msg[8] = {ATA_CMD_READ, lba, count, shm_handle};
    mod_send(ata_pid, msg);

    uint64_t reply[8];
    unsigned long long sender;
    do {
        sender = mod_recv_from(ata_pid, reply);
    } while (sender != ata_pid);

    return (reply[0] == 0) ? 0 : -1;
}

static uint32_t rd32(void* buf, uint32_t off) {
    uint8_t* b = (uint8_t*)buf;
    return (uint32_t)b[off] | ((uint32_t)b[off+1] << 8)
         | ((uint32_t)b[off+2] << 16) | ((uint32_t)b[off+3] << 24);
}

static uint64_t rd64(void* buf, uint32_t off) {
    uint8_t* b = (uint8_t*)buf;
    return (uint64_t)b[off] | ((uint64_t)b[off+1] << 8)
         | ((uint64_t)b[off+2] << 16) | ((uint64_t)b[off+3] << 24)
         | ((uint64_t)b[off+4] << 32) | ((uint64_t)b[off+5] << 40)
         | ((uint64_t)b[off+6] << 48) | ((uint64_t)b[off+7] << 56);
}

static int gpt_init(void) {
    ata_pid = find_module("ata_pio");
    if (!ata_pid) {
        debug_puts("GPT: ata_pio not found\n");
        return -1;
    }
    debug_puts("GPT: ata_pio pid=");
    debug_putu(ata_pid);
    debug_puts("\n");

    shm_handle = shm_create(1, SHM_ALL_RIGHTS);
    if (shm_handle == (unsigned long long)-1) {
        debug_puts("GPT: shm_create failed\n");
        return -1;
    }
    shm_vaddr = shm_attach(shm_handle);
    if (shm_vaddr == (unsigned long long)-1) {
        debug_puts("GPT: shm_attach failed\n");
        return -1;
    }

    if (ata_read(0, 1) != 0) {
        debug_puts("GPT: failed to read MBR\n");
        return -1;
    }

    uint8_t* mbr = (uint8_t*)shm_vaddr;
    if (mbr[0x1FE] != 0x55 || mbr[0x1FF] != 0xAA) {
        debug_puts("GPT: no MBR signature\n");
        return -1;
    }
    uint8_t ptype = mbr[0x1C2];
    if (ptype != 0xEE) {
        debug_puts("GPT: no protective MBR\n");
        return -1;
    }

    if (ata_read(1, 1) != 0) {
        debug_puts("GPT: failed to read GPT header\n");
        return -1;
    }

    uint8_t* hdr = (uint8_t*)shm_vaddr;
    uint64_t sig = rd64(hdr, 0);
    if (sig != 0x5452415020494645ULL) {
        debug_puts("GPT: bad signature\n");
        return -1;
    }

    uint64_t part_lba  = rd64(hdr, 72);
    uint32_t part_ents = rd32(hdr, 80);
    uint32_t part_size = rd32(hdr, 84);

    if (part_ents > 64) part_ents = 64;

    uint32_t sectors_needed = (part_ents * part_size + ATA_SECTORS - 1) / ATA_SECTORS;

    for (uint32_t s = 0; s < sectors_needed; s++) {
        if (ata_read(part_lba + s, 1) != 0) {
            debug_puts("GPT: failed to read partition entry sector\n");
            return -1;
        }
        uint8_t* ent = (uint8_t*)shm_vaddr;
        for (uint32_t i = 0; i < ATA_SECTORS / part_size && part_count < (int)part_ents; i++) {
            uint32_t off = i * part_size;
            int empty = 1;
            for (int b = 0; b < 16; b++) {
                if (ent[off + b]) { empty = 0; break; }
            }
            if (empty) continue;

            for (int b = 0; b < 16; b++)
                part_guid[part_count][b] = ent[off + b];
            part_start[part_count] = rd64(ent, off + 32);
            part_end[part_count]   = rd64(ent, off + 40);
            part_count++;
        }
    }

    debug_puts("GPT: partitions found=");
    debug_putu(part_count);
    debug_puts("\n");
    return 0;
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* args = (uint64_t*)msg;
    uint64_t cmd = args[0];
    uint64_t reply[8] = {0};

    switch (cmd) {
    case GPT_CMD_GET_COUNT:
        reply[0] = 0;
        reply[1] = part_count;
        break;

    case GPT_CMD_GET_PART: {
        uint64_t idx = args[1];
        if (idx >= (uint64_t)part_count) {
            reply[0] = -1;
            break;
        }
        reply[0] = 0;
        reply[1] = part_start[idx];
        reply[2] = part_end[idx];
        for (int i = 0; i < 4; i++)
            reply[3 + i] = ((uint64_t*)part_guid[idx])[i];
        break;
    }

    case GPT_CMD_FIND_ESP: {
        int found = -1;
        for (int i = 0; i < part_count; i++) {
            if (guid_cmp(part_guid[i], esp_type_guid)) {
                found = i; break;
            }
        }
        if (found >= 0) {
            reply[0] = 0;
            reply[1] = part_start[found];
            reply[2] = part_end[found];
        } else {
            reply[0] = -1;
        }
        break;
    }

    default:
        reply[0] = -1;
        break;
    }

    mod_send(sender, reply);
}

__attribute__((section(".text.start")))
void _start(void) {
    debug_puts("GPT: init\n");

    if (gpt_init() != 0)
        debug_puts("GPT: init failed, continuing anyway\n");
    else
        debug_puts("GPT: ready\n");

    while (1) {
        unsigned char msg[64];
        unsigned long long sender = mod_recv(msg);
        if (sender != (unsigned long long)-1)
            handle_ipc(msg, sender);
        else
            asm volatile("pause");
    }
}
