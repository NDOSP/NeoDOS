#ifndef VFS
#define VFS

#include <stdint.h>

typedef struct {
    uint64_t fd;
    uint64_t file_size;
} FileDetail;

typedef struct {
    uint64_t pid;
    uint64_t (*mount)(const char letter, uint64_t pid);
    FileDetail (*open)(const char* path);
    FileDetail (*create)(const char* path);
} VFSModTable;

#endif // VFS_H