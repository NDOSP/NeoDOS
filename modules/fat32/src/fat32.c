#include "modstd.h"
#include <stdint.h>

REGISTER_MODULE("fat32");

typedef struct { uint64_t pid; char name[64]; } ModEntry;

#define CMD_OPEN  1
#define CMD_READ  2

#define FS_OPEN  0x101
#define FS_READ  0x102
#define FS_CLOSE 0x104
#define VFS_MOUNT 1

#define ATA_READ 1
#define GPT_FIND_ESP 3

static uint64_t ata_pid = 0;
static uint64_t gpt_pid = 0;

static unsigned long long m_shm = 0;
static unsigned long long m_vaddr = 0;

static uint64_t vfs_pid = 0;
static uint64_t my_pid = 0;

#define MAX_SHM_CACHE 8
static unsigned long long shm_handles[MAX_SHM_CACHE];
static unsigned long long shm_vaddrs[MAX_SHM_CACHE];
static int shm_cache_count = 0;

static uint64_t part_lba = 0;
static uint16_t bps = 512;
static uint8_t  spc = 1;
static uint32_t rsvd = 0;
static uint8_t  nfat = 2;
static uint32_t spf = 0;
static uint64_t data_lba = 0;
static uint32_t root_clust = 0;

static uint64_t find_mod(const char* name) {
    unsigned long long buf = mod_syscall(SYSCALL_ALLOC_PAGES, 2, 0, 0);
    if (buf == 0 || buf == (unsigned long long)-1) return 0;
    int cnt = mod_list((void*)buf, 64);
    ModEntry* e = (ModEntry*)buf;
    for (int i = 0; i < cnt; i++) {
        int m = 1;
        for (int j = 0; name[j]; j++) { if (e[i].name[j] != name[j]) { m = 0; break; } }
        if (m && e[i].pid) return e[i].pid;
    }
    return 0;
}

static int send_cmd(uint64_t pid, uint64_t* msg, uint64_t* reply) {
    mod_send(pid, msg);
    unsigned long long snd;
    do { snd = mod_recv_from(pid, reply); } while (snd != pid);
    return (reply[0] == 0) ? 0 : -1;
}

static int ata_read(uint64_t lba, uint32_t cnt) {
    uint64_t m[8] = {ATA_READ, lba, cnt, m_shm};
    uint64_t r[8];
    return send_cmd(ata_pid, m, r);
}

static uint32_t rd32(uint8_t* b, uint32_t o) {
    return (uint32_t)b[o] | ((uint32_t)b[o+1]<<8) | ((uint32_t)b[o+2]<<16) | ((uint32_t)b[o+3]<<24);
}
static uint16_t rd16(uint8_t* b, uint32_t o) {
    return (uint16_t)b[o] | ((uint16_t)b[o+1]<<8);
}

static uint32_t next_cluster(uint32_t cl) {
    uint32_t fat_off = cl * 4;
    uint32_t fat_sec = rsvd + fat_off / bps;
    if (ata_read(part_lba + fat_sec, 1) != 0) return 0x0FFFFFFF;
    return rd32((uint8_t*)m_vaddr, fat_off % bps) & 0x0FFFFFFF;
}

static int find_in_dir(uint32_t dir_cl, const char* name, uint32_t* out_cl, uint32_t* out_sz) {
    int nlen = 0;
    while (name[nlen]) nlen++;

    char name8[8], ext[3];
    int ni = 0, ei = 0, dot = 0;
    for (int i = 0; i < nlen; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c == '.') { dot = 1; continue; }
        if (!dot) { if (ni < 8) name8[ni++] = c; }
        else { if (ei < 3) ext[ei++] = c; }
    }
    while (ni < 8) name8[ni++] = ' ';
    while (ei < 3) ext[ei++] = ' ';

    uint32_t cl = dir_cl;
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        uint64_t lba = part_lba + data_lba + (uint64_t)(cl - 2) * spc;
        for (uint32_t s = 0; s < spc; s++) {
            if (ata_read(lba + s, 1) != 0) return -1;
            uint8_t* d = (uint8_t*)m_vaddr;
            for (int i = 0; i < bps / 32; i++) {
                uint8_t* e = d + i * 32;
                if (e[0] == 0x00) return -1;
                if (e[0] == 0xE5 || e[11] == 0x0F) continue;
                int match = 1;
                for (int j = 0; j < 8; j++) if (e[j] != (uint8_t)name8[j]) { match = 0; break; }
                if (match) for (int j = 0; j < 3; j++) if (e[8+j] != (uint8_t)ext[j]) { match = 0; break; }
                if (!match) continue;
                *out_cl = (uint32_t)rd16(e, 20) << 16 | rd16(e, 26);
                *out_sz = rd32(e, 28);
                return (e[11] & 0x10) ? 1 : 0;
            }
        }
        cl = next_cluster(cl);
    }
    return -1;
}

static int resolve_path(const char* path, uint32_t* out_cl, uint32_t* out_sz) {
    uint32_t cl = root_clust;
    int idx = 0;
    while (path[idx] == '/' || path[idx] == '\\') idx++;
    if (!path[idx]) { *out_cl = root_clust; *out_sz = 0; return 1; }

    char comp[256];
    while (path[idx]) {
        int ci = 0;
        while (path[idx] && path[idx] != '/' && path[idx] != '\\' && ci < 255)
            comp[ci++] = path[idx++];
        comp[ci] = '\0';
        while (path[idx] == '/' || path[idx] == '\\') idx++;

        uint32_t next_cl, next_sz;
        int r = find_in_dir(cl, comp, &next_cl, &next_sz);
        if (r < 0) return -1;
        if (r == 1) {
            cl = next_cl;
            if (!path[idx]) { *out_cl = cl; *out_sz = 0; return 1; }
            continue;
        }
        *out_cl = next_cl;
        *out_sz = next_sz;
        return 0;
    }
    return -1;
}

static int init_fs(void) {
    debug_puts("FAT32: find ata\n");
    ata_pid = find_mod("ata_pio");
    debug_puts("FAT32: find gpt\n");
    gpt_pid = find_mod("gpt");
    if (!ata_pid || !gpt_pid) { debug_puts("FAT32: mod not found\n"); return -1; }

    debug_puts("FAT32: shm_create\n");
    m_shm = shm_create(2, SHM_ALL_RIGHTS);
    if (m_shm == (unsigned long long)-1) { debug_puts("FAT32: shm_create fail\n"); return -1; }
    debug_puts("FAT32: shm_attach\n");
    m_vaddr = shm_attach(m_shm);
    if (m_vaddr == (unsigned long long)-1) { debug_puts("FAT32: shm_attach fail\n"); return -1; }

    debug_puts("FAT32: send to gpt\n");
    uint64_t m[8] = {GPT_FIND_ESP};
    uint64_t r[8];
    if (send_cmd(gpt_pid, m, r) != 0) { debug_puts("FAT32: gpt no esp\n"); return -1; }
        debug_puts("FAT32: esp lba=");
        debug_putu(r[1]);
        debug_puts("\n");
    part_lba = r[1];

    debug_puts("FAT32: read bpb\n");
    if (ata_read(part_lba, 1) != 0) { debug_puts("FAT32: bpb read fail\n"); return -1; }
    uint8_t* b = (uint8_t*)m_vaddr;

    if (rd16(b, 11) != 512) { debug_puts("FAT32: bad bps\n"); return -1; }
    bps = 512;
    spc = b[13];
    rsvd = rd16(b, 14);
    nfat = b[16];
    spf = rd32(b, 36);
    root_clust = rd32(b, 44);
    data_lba = rsvd + (uint64_t)nfat * spf;

    my_pid = mod_syscall(SYSCALL_GETPID, 0, 0, 0);
    vfs_pid = find_mod("vfs");
    if (vfs_pid) {
        uint64_t m[8] = {VFS_MOUNT, 'C', my_pid};
        uint64_t r[8];
        send_cmd(vfs_pid, m, r);
        debug_puts("FAT32: registered with VFS as C:\n");
    } else {
        debug_puts("FAT32: VFS not found, skipping registration\n");
    }
    return 0;
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* a = (uint64_t*)msg;
    uint64_t reply[8] = {0};

    switch (a[0]) {
    case FS_READ:
    case CMD_READ: {
        uint32_t cl = (uint32_t)a[1];
        uint32_t off = (uint32_t)a[2];
        unsigned long long shm = a[3];
        uint32_t maxb = (uint32_t)a[4];
        if (maxb == 0) { reply[0] = (uint64_t)-1; break; }

        // Cache SHM mapping — attach once per handle
        unsigned long long buf_v = 0;
        int found = 0;
        for (int i = 0; i < shm_cache_count; i++) {
            if (shm_handles[i] == shm) { buf_v = shm_vaddrs[i]; found = 1; break; }
        }
        if (!found) {
            if (shm_cache_count >= MAX_SHM_CACHE) { reply[0] = -1; break; }
            buf_v = shm_attach(shm);
            if (buf_v == (unsigned long long)-1) { reply[0] = (uint64_t)-1; break; }
            shm_handles[shm_cache_count] = shm;
            shm_vaddrs[shm_cache_count] = buf_v;
            shm_cache_count++;
        }
        uint8_t* dst = (uint8_t*)buf_v;

        uint32_t cl_skip = off / ((uint32_t)spc * bps);
        uint32_t cl_off = off % ((uint32_t)spc * bps);
        uint32_t written = 0;
        int err = 0;

        uint32_t cur = cl;
        for (uint32_t k = 0; k < cl_skip && !err; k++) {
            cur = next_cluster(cur);
            if (cur >= 0x0FFFFFF8) err = 1;
        }
        if (err) { reply[0] = (uint64_t)-1; break; }

        while (!err && cur >= 2 && cur < 0x0FFFFFF8 && written < maxb) {
            uint64_t lba = part_lba + data_lba + (uint64_t)(cur - 2) * spc;
            uint32_t remaining = (uint32_t)spc * bps - cl_off;
            if (remaining > maxb - written) remaining = maxb - written;

            for (uint32_t s = 0; s < spc && remaining > 0 && written < maxb; s++) {
                if (ata_read(lba + s, 1) != 0) { err = 1; break; }
                uint8_t* src = (uint8_t*)m_vaddr;
                uint32_t start = (s == 0) ? cl_off : 0;
                uint32_t chunk = bps - start;
                if (chunk > remaining) chunk = remaining;
                for (uint32_t i = 0; i < chunk; i++)
                    dst[written++] = src[start + i];
                remaining -= chunk;
            }
            cl_off = 0;
            cur = next_cluster(cur);
        }

        reply[0] = err ? (uint64_t)-1 : 0;
        reply[1] = written;
        break;
    }

    case FS_OPEN:
    case CMD_OPEN: {
        const char* path = (const char*)(msg + 8);
        uint32_t cl, sz;
        int r = resolve_path(path, &cl, &sz);
        if (r >= 0) { reply[0] = 0; reply[1] = cl; reply[2] = sz; }
        else reply[0] = -1;
        break;
    }

    case FS_CLOSE:
        reply[0] = 0;
        break;

    default:
        reply[0] = -1;
    }

    mod_send(sender, reply);
}

void init(void) {
    debug_puts("FAT32: init\n");
    my_pid = MY_PID;
    if (init_fs() != 0) {
        debug_puts("FAT32: init failed, continuing anyway\n");
    } else {
        debug_puts("FAT32: ready\n");
    }
}

void loop(void) {
    unsigned char msg[64];
    unsigned long long snd = mod_recv(msg);
    if (snd != (unsigned long long)-1)
        handle_ipc(msg, snd);
    else
        asm volatile("pause");
}
