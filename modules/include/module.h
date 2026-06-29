#ifndef MODULE_FORMAT_H
#define MODULE_FORMAT_H

#define MOD_MAGIC   0x444F4D
#define MOD_VERSION 1
#define MOD_NAME_MAX 64
#define MOD_MAX_DEPS 16
#define MOD_MAX_IMPORTS 64

typedef struct ModInfo {
    unsigned int magic;
    unsigned int version;
    char         name[MOD_NAME_MAX];
    unsigned long long init_off;
    unsigned long long exit_off;
    unsigned int dep_count;
    unsigned int import_count;
    unsigned long long deps_off;
    unsigned long long imports_off;
} __attribute__((packed)) ModInfo;

#define MODINFO(name) \
    static ModInfo __attribute__((section(".modinfo"), used)) __g_modinfo = { \
        MOD_MAGIC, MOD_VERSION, name, 0, 0, 0, 0, 0, 0 \
    }

#endif
