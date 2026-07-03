#ifndef MODS
#define MODS

#include "std.h"

typedef struct { uint64_t pid; char name[64]; } ModEntry;

static uint64_t find_mod(const char* name) {
    unsigned long long buf = alloc(2);
    if (buf == 0 || buf == (unsigned long long)-1) return 0;
    int cnt = list_mods((void*)buf, 64);
    ModEntry* e = (ModEntry*)buf;
    for (int i = 0; i < cnt; i++) {
        int m = 1;
        for (int j = 0; name[j]; j++) { if (e[i].name[j] != name[j]) { m = 0; break; } }
        if (m && e[i].pid) return e[i].pid;
    }
    return 0;
}

#define MODTABLE_SIZE 64
#define MAX_CONNECTED_MODS 64

typedef struct {
    uint64_t pid;
    uint64_t functions[MODTABLE_SIZE - 1];
} ModTable;

#define MODTABLE_CALL(modtable, function, ...) \
    ((typeof(function))((uint64_t)modtable + (uint64_t)function))(__VA_ARGS__)

#define MODTABLE_BYTES MODTABLE_SIZE * sizeof(ModTable)
#define MOD_GET_TABLE 0xFEEFDEED3883EDDE

static ModTable* get_mod_table(const char* name) {
    uint64_t mod_pid = find_mod(name);
    uint64_t mes[8] = {0};

    mes[0] = MOD_GET_TABLE;
    send(mod_pid, mes);

    uint64_t shm;
    while(1) {
        unsigned char msg[64];
        unsigned long long snd = recv(msg);

        if (snd == mod_pid) {
            shm = *(uint64_t*)msg;
            break;
        } else
            asm volatile("pause");
    }

    return (ModTable*)(shm_attach(shm) + 16); // TODO: Remove + 16 (for now its only working with +16)
}

#endif // MODS