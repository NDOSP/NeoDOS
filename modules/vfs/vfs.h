#ifndef VFS
#define VFS

#include <stdint.h>

typedef struct {
    uint64_t pid;
    uint64_t (*open)(const char* path);
} VFSModTable;

#endif // VFS_H