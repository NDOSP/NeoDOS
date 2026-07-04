#include "modstd.h"
#include "vfs/vfs.h"
#include <stdint.h>

REGISTER_MODULE("ramfs")

#define FS_OPEN   0x101
#define FS_READ   0x102
#define FS_WRITE  0x103
#define FS_CLOSE  0x104
#define FS_CREATE 0x105
#define FS_DELETE 0x106

#define VFS_MOUNT 1

#define MAX_FILES    32
#define MAX_NAME     64
#define MAX_PAGES    16

typedef struct {
    int used;
    char name[MAX_NAME];
    unsigned long long pages[MAX_PAGES];
    int num_pages;
    uint32_t size;
} RamFile;

static RamFile files[MAX_FILES];
static uint64_t my_pid = 0;

static int find_file(const char* name) {
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i].used) {
            int eq = 1;
            for (int j = 0; name[j] || files[i].name[j]; j++)
                if (name[j] != files[i].name[j]) { eq = 0; break; }
            if (eq) return i;
        }
    return -1;
}

static int alloc_file_slot(void) {
    for (int i = 0; i < MAX_FILES; i++)
        if (!files[i].used) return i;
    return -1;
}

// Strip leading separators from name, write into out (max MAX_NAME).
static void clean_name(const char* path, char* out) {
    int src = 0;
    while (path[src] == '/' || path[src] == '\\') src++;
    int di = 0;
    while (path[src] && di < MAX_NAME - 1) {
        out[di++] = path[src++];
    }
    out[di] = '\0';
}

static int alloc_page_for(RamFile* f) {
    if (f->num_pages >= MAX_PAGES) return -1;
    unsigned long long p = mod_syscall(SYSCALL_ALLOC_PAGES, 1, 0, 0);
    if (p == 0 || p == (unsigned long long)-1) return -1;
    f->pages[f->num_pages++] = p;
    return 0;
}

static void free_pages(RamFile* f) {
    for (int i = 0; i < f->num_pages; i++)
        mod_syscall(SYSCALL_FREE_PAGES, f->pages[i], 1, 0);
    f->num_pages = 0;
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* a = (uint64_t*)msg;
    uint64_t reply[8] = {0};

    switch (a[0]) {
    case FS_OPEN:
    case FS_CREATE: {
        const char* path = (const char*)(msg + 8);
        char name[MAX_NAME];
        clean_name(path, name);
        if (!name[0]) { reply[0] = -1; break; }

        int idx = find_file(name);
        if (a[0] == FS_OPEN) {
            if (idx < 0) { reply[0] = -1; break; }
            reply[0] = 0;
            reply[1] = idx;
            reply[2] = files[idx].size;
        } else {
            if (idx >= 0) { reply[0] = -1; break; }
            int fi = alloc_file_slot();
            if (fi < 0) { reply[0] = -1; break; }
            files[fi].used = 1;
            int ci = 0;
            while (name[ci] && ci < MAX_NAME - 1) {
                files[fi].name[ci] = name[ci];
                ci++;
            }
            files[fi].name[ci] = '\0';
            files[fi].num_pages = 0;
            files[fi].size = 0;
            reply[0] = 0;
            reply[1] = fi;
            reply[2] = 0;
        }
        break;
    }

    case FS_READ: {
        int fi = (int)a[1];
        uint32_t off = (uint32_t)a[2];
        unsigned long long shm = a[3];
        uint32_t maxb = (uint32_t)a[4];

        if (fi < 0 || fi >= MAX_FILES || !files[fi].used) {
            reply[0] = (uint64_t)-1;
            break;
        }
        RamFile* f = &files[fi];
        if (off >= f->size || maxb == 0) { reply[0] = 0; reply[1] = 0; break; }

        unsigned long long buf_v = shm_attach(shm);
        if (buf_v == (unsigned long long)-1) { reply[0] = (uint64_t)-1; break; }
        uint8_t* dst = (uint8_t*)buf_v;

        uint32_t page_off = off % PAGE_SIZE;
        uint32_t written = 0;
        uint32_t pi = off / PAGE_SIZE;

        while (pi < (uint32_t)f->num_pages && written < maxb && off + written < f->size) {
            uint8_t* src = (uint8_t*)f->pages[pi];
            uint32_t chunk = PAGE_SIZE - page_off;
            uint32_t remain_file = f->size - (off + written);
            if (chunk > remain_file) chunk = remain_file;
            if (chunk > maxb - written) chunk = maxb - written;
            for (uint32_t i = 0; i < chunk; i++)
                dst[written++] = src[page_off + i];
            pi++;
            page_off = 0;
        }

        reply[0] = 0;
        reply[1] = written;
        break;
    }

    case FS_WRITE: {
        int fi = (int)a[1];
        uint32_t off = (uint32_t)a[2];
        unsigned long long shm = a[3];
        uint32_t maxb = (uint32_t)a[4];

        if (fi < 0 || fi >= MAX_FILES || !files[fi].used || maxb == 0) {
            reply[0] = (uint64_t)-1;
            break;
        }
        RamFile* f = &files[fi];

        unsigned long long buf_v = shm_attach(shm);
        if (buf_v == (unsigned long long)-1) { reply[0] = (uint64_t)-1; break; }
        uint8_t* src = (uint8_t*)buf_v;

        uint32_t written = 0;
        uint32_t pi = off / PAGE_SIZE;
        uint32_t page_off = off % PAGE_SIZE;

        while (written < maxb) {
            // Allocate pages on demand
            while (pi >= (uint32_t)f->num_pages) {
                if (alloc_page_for(f) != 0) { reply[0] = (uint64_t)-1; goto write_done; }
            }
            uint8_t* dst = (uint8_t*)f->pages[pi];
            uint32_t chunk = PAGE_SIZE - page_off;
            if (chunk > maxb - written) chunk = maxb - written;
            for (uint32_t i = 0; i < chunk; i++)
                dst[page_off + i] = src[written++];
            pi++;
            page_off = 0;
        }

        { uint32_t end = off + written; if (end > f->size) f->size = end; }

        reply[0] = 0;
        reply[1] = written;

    write_done:
        break;
    }

    case FS_CLOSE:
        reply[0] = 0;
        break;

    case FS_DELETE: {
        const char* path = (const char*)(msg + 8);
        char name[MAX_NAME];
        clean_name(path, name);
        int idx = find_file(name);
        if (idx < 0) { reply[0] = -1; break; }
        free_pages(&files[idx]);
        files[idx].used = 0;
        reply[0] = 0;
        break;
    }

    default:
        reply[0] = -1;
    }

    send(sender, reply);
}

void init(void) {
    my_pid = get_pid();

    VFSModTable* vfs = (VFSModTable*)get_mod_table("vfs");
    if (vfs) {
        MODTABLE_CALL(vfs, vfs->mount, 'T', my_pid);
    }
}

void loop(void) {
    while (1) {
        unsigned char msg[64];
        unsigned long long snd = mod_recv(msg);
        if (snd != (unsigned long long)-1)
            handle_ipc(msg, snd);
        else
            asm volatile("pause");
    }
}
