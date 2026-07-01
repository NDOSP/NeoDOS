#include "modstd.h"
#include <stdint.h>

REGISTER_MODULE("vfs");

#define VFS_MOUNT   1
#define VFS_UNMOUNT 2
#define VFS_OPEN    3
#define VFS_READ    4
#define VFS_WRITE   5
#define VFS_CLOSE   6
#define VFS_CREATE  7
#define VFS_DELETE  8

#define FS_OPEN   0x101
#define FS_READ   0x102
#define FS_WRITE  0x103
#define FS_CLOSE  0x104
#define FS_CREATE 0x105
#define FS_DELETE 0x106

#define MAX_DRIVES 26
#define MAX_FILES  64

typedef struct {
    int used;
    char letter;
    uint64_t fs_pid;
} DriveEntry;

typedef struct {
    int used;
    uint64_t fs_pid;
    uint64_t fs_handle;
    uint64_t size;
} FileEntry;

static DriveEntry drives[MAX_DRIVES];
static FileEntry files[MAX_FILES];

static int find_drive(char letter) {
    for (int i = 0; i < MAX_DRIVES; i++)
        if (drives[i].used && drives[i].letter == letter)
            return i;
    return -1;
}

static int alloc_drive(void) {
    for (int i = 0; i < MAX_DRIVES; i++)
        if (!drives[i].used) return i;
    return -1;
}

static int alloc_file(void) {
    for (int i = 0; i < MAX_FILES; i++)
        if (!files[i].used) return i;
    return -1;
}

static int send_cmd(uint64_t pid, uint64_t* msg, uint64_t* reply) {
    mod_send(pid, msg);
    unsigned long long snd;
    do { snd = mod_recv_from(pid, reply); } while (snd != pid);
    return (reply[0] == 0) ? 0 : -1;
}

// Parse "X:..." from msg+8, forward cmd to the drive's FS module.
// Returns 0 on success with reply populated; -1 on error.
static int forward_to_fs(unsigned char* msg, uint64_t fs_cmd,
                         uint64_t* r, int* out_di) {
    const char* path = (const char*)(msg + 8);
    char letter = path[0];
    if (letter >= 'a' && letter <= 'z') letter -= 32;
    if (letter < 'A' || letter > 'Z' || path[1] != ':') return -1;
    int di = find_drive(letter);
    if (di < 0) return -1;

    uint64_t m[8];
    m[0] = fs_cmd;
    int pi = 0;
    int src = 2;
    while (path[src] && pi < 55) {
        ((char*)m)[8 + pi] = path[src];
        pi++; src++;
    }
    ((char*)m)[8 + pi] = '\0';

    if (send_cmd(drives[di].fs_pid, m, r) != 0) return -1;
    if (out_di) *out_di = di;
    return 0;
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* a = (uint64_t*)msg;
    uint64_t reply[8] = {0};

    switch (a[0]) {
    case VFS_MOUNT: {
        char letter = (char)(a[1] & 0xFF);
        uint64_t fs_pid = a[2];
        if (letter >= 'a' && letter <= 'z') letter -= 32;
        if (letter < 'A' || letter > 'Z') { reply[0] = -1; break; }
        int idx = alloc_drive();
        if (idx < 0) { reply[0] = -1; break; }
        drives[idx].used = 1;
        drives[idx].letter = letter;
        drives[idx].fs_pid = fs_pid;
        reply[0] = 0;
        break;
    }

    case VFS_UNMOUNT: {
        char letter = (char)(a[1] & 0xFF);
        if (letter >= 'a' && letter <= 'z') letter -= 32;
        int idx = find_drive(letter);
        if (idx < 0) { reply[0] = -1; break; }
        drives[idx].used = 0;
        reply[0] = 0;
        break;
    }

    case VFS_CREATE:
    case VFS_OPEN: {
        uint64_t fs_cmd = (a[0] == VFS_OPEN) ? FS_OPEN : FS_CREATE;
        uint64_t r[8];
        int di;
        if (forward_to_fs(msg, fs_cmd, r, &di) != 0) { reply[0] = -1; break; }

        int fi = alloc_file();
        if (fi < 0) { reply[0] = -1; break; }
        files[fi].used = 1;
        files[fi].fs_pid = drives[di].fs_pid;
        files[fi].fs_handle = r[1];
        files[fi].size = r[2];

        reply[0] = 0;
        reply[1] = fi;
        reply[2] = r[2];
        break;
    }

    case VFS_READ: {
        uint64_t handle = a[1];
        uint64_t offset = a[2];
        unsigned long long shm = a[3];
        uint64_t count = a[4];

        if (handle >= MAX_FILES || !files[handle].used) { reply[0] = -1; break; }
        FileEntry* f = &files[handle];

        uint64_t m[8] = {FS_READ, f->fs_handle, offset, shm, count};
        uint64_t r[8];
        if (send_cmd(f->fs_pid, m, r) != 0) { reply[0] = -1; break; }
        reply[0] = 0;
        reply[1] = r[1];
        break;
    }

    case VFS_WRITE: {
        uint64_t handle = a[1];
        uint64_t offset = a[2];
        unsigned long long shm = a[3];
        uint64_t count = a[4];

        if (handle >= MAX_FILES || !files[handle].used) { reply[0] = -1; break; }
        FileEntry* f = &files[handle];

        uint64_t m[8] = {FS_WRITE, f->fs_handle, offset, shm, count};
        uint64_t r[8];
        if (send_cmd(f->fs_pid, m, r) != 0) { reply[0] = -1; break; }
        reply[0] = 0;
        reply[1] = r[1];
        break;
    }

    case VFS_CLOSE: {
        uint64_t handle = a[1];
        if (handle >= MAX_FILES || !files[handle].used) { reply[0] = -1; break; }
        FileEntry* f = &files[handle];

        uint64_t m[8] = {FS_CLOSE, f->fs_handle};
        uint64_t r[8];
        send_cmd(f->fs_pid, m, r);

        files[handle].used = 0;
        reply[0] = 0;
        break;
    }

    case VFS_DELETE: {
        uint64_t r[8];
        if (forward_to_fs(msg, FS_DELETE, r, 0) != 0) { reply[0] = -1; break; }
        reply[0] = 0;
        break;
    }

    default:
        reply[0] = -1;
    }

    mod_send(sender, reply);
}

void init(void) {
    return;
}

void loop(void) {
    unsigned char msg[64];
    unsigned long long snd = mod_recv(msg);

    if (snd != (unsigned long long)-1)
        handle_ipc(msg, snd);
    else
        asm volatile("pause");
}
