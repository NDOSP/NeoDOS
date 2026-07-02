#include "modlib.h"

#define EXECV 2
#define NDR_HEADER_SIZE 32
#define NDR_TYPE_UTF8 2

typedef struct { uint64_t pid; char name[64]; } ModEntry;

static uint64_t find_mod(const char* name) {
    unsigned long long buf = mod_syscall(SYSCALL_ALLOC_PAGES, 2, 0, 0);
    if (buf == 0 || buf == (unsigned long long)-1) return 0;
    int cnt = mod_list((void*)buf, 64);
    ModEntry* e = (ModEntry*)buf;
    for (int i = 0; i < cnt; i++) {
        int m = 1;
        for (int j = 0; name[j]; j++)
            if (e[i].name[j] != name[j]) { m = 0; break; }
        if (m && e[i].pid) return e[i].pid;
    }
    return 0;
}

// Read a string value from NDR registry by entry name.
// Returns 1 on success, 0 on failure.
static int ndr_get_string(const char* name, char* out, int max_len) {
    unsigned long long sz = ndr_size();
    if (sz == (unsigned long long)-1 || sz < NDR_HEADER_SIZE + 16) return 0;

    unsigned long long pages = (sz + 4095) / 4096;
    unsigned long long buf = mod_syscall(SYSCALL_ALLOC_PAGES, pages, 0, 0);
    if (buf == 0 || buf == (unsigned long long)-1) return 0;

    unsigned long long copied = ndr_copy((void*)buf, 0, sz);
    if (copied == (unsigned long long)-1) return 0;

    unsigned char* data = (unsigned char*)buf;
    unsigned long long pos = NDR_HEADER_SIZE;
    unsigned long long num = *(unsigned long long*)(data + pos);
    pos += 16;

    unsigned long long nl = 0;
    while (name[nl]) nl++;

    for (unsigned long long i = 0; i < num; i++) {
        unsigned long long off = *(unsigned long long*)(data + pos + 8);
        pos += 32;

        unsigned long long ep = off;
        unsigned long long ts = *(unsigned long long*)(data + ep);
        unsigned long long enl = *(unsigned long long*)(data + ep + 16);
        ep += 32;

        if (enl != nl) continue;
        int match = 1;
        for (unsigned long long j = 0; j < nl; j++)
            if (data[ep + j] != (unsigned char)name[j]) { match = 0; break; }
        if (!match) continue;

        ep += enl;
        unsigned int et = *(unsigned int*)(data + ep);
        if (et != NDR_TYPE_UTF8) return 0;
        ep += 12;

        unsigned long long br = ep - off;
        unsigned long long ds = ts - br;
        unsigned long long cl = ds < (unsigned long long)max_len - 1 ? ds : (unsigned long long)max_len - 1;
        for (unsigned long long j = 0; j < cl; j++)
            out[j] = (char)data[ep + j];
        out[cl] = 0;
        return 1;
    }
    return 0;
}

__attribute__((noreturn, section(".text.start")))
void _start(void) {
    debug_puts("INIT: launching shell\n");

    char shell_path[64];
    unsigned long long si;
    for (si = 0; si < 21; si++) shell_path[si] = "C:\\NEODOS\\BIN\\shell"[si];
    shell_path[21] = 0;

    if (ndr_get_string("ShellPath", shell_path, 64)) {
        debug_puts("INIT: shell path from NDR\n");
    } else {
        debug_puts("INIT: using default shell path\n");
    }

    uint64_t elf_pid = find_mod("elf");
    if (!elf_pid) {
        debug_puts("INIT: elf mod not found\n");
        exit();
    }

    uint64_t msg[8];
    msg[0] = EXECV;
    int mi = 0;
    while (shell_path[mi] && mi < 55) { ((char*)msg)[8 + mi] = shell_path[mi]; mi++; }
    ((char*)msg)[8 + mi] = '\0';

    uint64_t r[8];

    for (int tries = 0; tries < 200; tries++) {
        mod_send(elf_pid, msg);
        unsigned long long snd;
        do { snd = mod_recv(r); } while (snd != elf_pid);
        if (r[0] == 0) {
            debug_puts("INIT: shell launched, exiting\n");
        }
    }

    debug_puts("INIT: exec shell failed\n");
    exit();
}
