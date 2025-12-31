#ifndef NDR_BOOT_H
#define NDR_BOOT_H

#include "file.h"

typedef enum {
    T_NODE = 0,
    T_NUM = 1,
    T_UTF8 = 2,
    T_UTF16 = 3,
    T_NULL = 0xFFFF
} EntryType;

typedef union {
    UINT64 num;
    CHAR8* str8;
    CHAR16* str16;
} EntryValue;

typedef struct ConfigNode ConfigNode;

struct ConfigNode {
    CHAR8* name;
    EntryType entryType;

    UINT8* value;
    UINTN valueSize;

    ConfigNode* next;
    ConfigNode* child;
};

ConfigNode* ConfigNodeInit(CHAR8 *name, EntryType entryType, UINT8 *value, UINTN valueSize);
void ConfigNodeAddChild(ConfigNode *self, ConfigNode *child);
const ConfigNode* ConfigNodeFindAny(const ConfigNode *self, const CHAR8 *name);
BOOLEAN ConfigNodeGetValue(const ConfigNode *node, EntryValue *out);
BOOLEAN ConfigNodeGetValueByPath(const ConfigNode *root, const CHAR8 *path, EntryValue *out);
UINT64 ConfigNodeGetU64(ConfigNode *root, const CHAR8 *path, UINT64 def);
CHAR8* ConfigNodeGetStr8(ConfigNode *root, const CHAR8 *path, CHAR8 *def);
CHAR16* ConfigNodeGetStr16(ConfigNode *root, const CHAR8 *path, CHAR16 *def);

EFI_STATUS getConfig(CHAR16* path, ConfigNode*** output);

#endif // NDR_BOOT_H