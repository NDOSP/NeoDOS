#ifndef REG_H
#define REG_H

#include <stdint.h>

#define REG_MAX_ENTRIES 256

typedef enum {
    REG_NODE = 0,
    REG_NUM  = 1,
    REG_UTF8 = 2,
    REG_UTF16 = 3,
} RegType;

typedef struct {
    uint64_t id;
    uint64_t rootId;
    const char* name;
    uint64_t nameLen;
    RegType type;
    const void* data;
    uint64_t dataSize;
} RegEntry;

void regInit(const void* data, uint64_t size);
uint64_t regReadU64(const char* path, uint64_t def);
const char* regReadStr8(const char* path, const char* def);

#endif
