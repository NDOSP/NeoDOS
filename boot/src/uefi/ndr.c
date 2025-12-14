#include "ndr.h"
#include <efilib.h>

typedef struct __attribute__((packed)) {
    UINT8 magic[4];
    UINT32 version;
    UINT32 rsvd1;
    UINT32 checksum;
    UINT64 totalSize;
    UINT64 rsvd;
} HEADER;

typedef struct __attribute__((packed)) {
    UINT64 entryId;
    UINT64 entryOffset;
    UINT64 rootId;
    UINT64 rsvd;
} REGISTRY_TABLE_ENTRY;

typedef struct __attribute__((packed)) {
    UINT64 numOfEntries;
    UINT32 rsvd;
    UINT32 checksum;
} REGISTRY_TABLE_HEADER;

typedef struct {
    UINT64 entrySize;
    UINT64 entryId;
    UINT64 entryNameSize;
    UINT64 rsvd1;
    CHAR8* entryName;
    UINT32 entryType;
    UINT32 rsvd2;
    UINT32 flags;
    UINTN dataSize;
    CHAR8* data;
} REGISTRY_ENTRY;

ConfigNode ConfigNodeInit(const CHAR8* name, EntryType entryType, UINT8* value, UINTN valueSize) {
    ConfigNode node;
    node.name = name;
    node.entryType = entryType;
    node.value = value;
    node.valueSize = valueSize;
    node.next = NULL;
    node.child = NULL;
    return node;
}

void ConfigNodeAddChild(ConfigNode* self, ConfigNode* child) {
    if (!self->child) {
        self->child = child;
        return;
    }

    ConfigNode *last = self->child;
    while (last->next)
        last = last->next;

    last->next = child;
}

const ConfigNode* ConfigNodeFindAny(const ConfigNode* self, const CHAR8* name) {
    if (!self) return NULL;
    if (!name) return NULL;
    
    Print(L"INFO: (config) ConfigNodeFindAny: self->name = %a, looking for %a\n", self->name, name);
    
    const ConfigNode* node = self;
    while (node) {
        if (node->name) {
            if (strcmpa(node->name, name) == 0) {
                return node;
            }
        } else {
            Print(L"WARNING: (config) ConfigNodeFindAny: node->name is NULL\n");
        }
        
        if (node->child) {
            const ConfigNode* found = ConfigNodeFindAny(node->child, name);
            if (found) return found;
        }
        node = node->next;
    }
    return NULL;
}

BOOLEAN ConfigNodeGetValue(const ConfigNode* node, EntryValue* out) {
    switch (node->entryType) {

        case T_NUM: {
            if (node->valueSize < 8) return FALSE;

            UINT64 num = 0;
            CopyMem(&num, node->value, 8);

            out->num = num;
            return TRUE;
        }

        case T_UTF8: {
            if (node->value == NULL || node->valueSize == 0) return FALSE;

            CHAR8 *copy = (CHAR8*)AllocatePool(node->valueSize + sizeof(CHAR8));
            if (!copy) return FALSE;

            CopyMem(copy, node->value, node->valueSize);
            copy[node->valueSize] = '\0';
            out->str8 = copy;
            return TRUE;
        }

        case T_UTF16: {
            if (node->valueSize == 0 || (node->valueSize & 1)) return FALSE;
            UINTN charCount = node->valueSize / sizeof(CHAR16);
            CHAR16 *copy = (CHAR16*)AllocatePool(node->valueSize + sizeof(CHAR16));
            if (!copy) return FALSE;

            CopyMem(copy, node->value, node->valueSize);
            copy[charCount] = L'\0';

            out->str16 = copy;
            return TRUE;
        }

        default:
            Print(L"WARNING: (config) Unknown ConfigNode value type\n");
            return FALSE;
    }
}

BOOLEAN ConfigNodeGetValueByPath(const ConfigNode *root, const CHAR8 *path, EntryValue *out) {
    Print(L"INFO: (config) Root name: %a\n", root->name);
    const ConfigNode *current = root;
    const CHAR8 *start = path;
    const CHAR8 *p = path;

    while (TRUE) {
        if (*p == '/' || *p == '\0') {
            UINTN len = p - start;
            if (len == 0) {
                if (*p == '/') {
                    start = p + 1;
                    continue;
                }
                return FALSE;
            }

            CHAR8* namebuf = (CHAR8*)AllocatePool(len + 1);
            if (!namebuf) return FALSE;
            
            CopyMem(namebuf, start, len);
            namebuf[len] = '\0';

            const ConfigNode* found = NULL;
            ConfigNode* child = current->child;
            while (child) {
                Print(L"INFO: (config) child=0x%lX, child->name=0x%lX, namebuf=0x%lX\n", child, child->name, namebuf);

                if (!child->name) {
                    Print(L"WARNING: child->name is NULL!\n");
                    child = child->next;
                    continue;
                }

                if (!namebuf) {
                    Print(L"ERROR: namebuf is NULL!\n");
                    break;
                }

                if (strcmpa(child->name, namebuf) == 0) {
                    found = child;
                    break;
                }
                child = child->next;
            }
            
            FreePool(namebuf);
            
            if (!found) {
                Print(L"WARNING: (config) Component %a not found\n", namebuf);
                return FALSE;
            }
            current = found;

            if (*p == '\0') break;
            start = p + 1;
        }
        
        if (*p == '\0') break;
        p++;
    }
    return ConfigNodeGetValue(current, out);
}

UINT64 ConfigNodeGetU64(ConfigNode *root, const CHAR8 *path, UINT64 def) {
    EntryValue val;
    if (ConfigNodeGetValueByPath(root, path, &val)) return val.num;
    return def;
}

CHAR8* ConfigNodeGetStr8(ConfigNode *root, const CHAR8 *path, CHAR8 *def) {
    EntryValue val;
    if (ConfigNodeGetValueByPath(root, path, &val)) return val.str8;
    return def;
}

CHAR16* ConfigNodeGetStr16(ConfigNode *root, const CHAR8 *path, CHAR16 *def) {
    EntryValue val;
    if (ConfigNodeGetValueByPath(root, path, &val)) return val.str16;
    return def;
}

const UINTN HEADER_CHECKSUM_OFFSET = 12;
const UINTN REGISTRY_TABLE_CRC_OFFSET = 12;

uint32_t crc32(const VOID* data, UINTN length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *buf = (const uint8_t *)data;

    for (UINTN i = 0; i < length; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

BOOLEAN verifyHeader(const HEADER *header) {
    const uint8_t *hdr_bytes = (const uint8_t*)header;

    uint8_t buf[sizeof(HEADER)];

    for (UINTN i = 0; i < sizeof(HEADER); i++)
        buf[i] = hdr_bytes[i];

    for (UINTN i = HEADER_CHECKSUM_OFFSET; i < HEADER_CHECKSUM_OFFSET + 4; i++)
        buf[i] = 0;

    uint32_t calc = crc32(buf, sizeof(HEADER));

    Print(L"INFO: (config) Header CRC file=0x%08X calc=0x%08X\n", header->checksum, calc);

    return calc == header->checksum;
}

EFI_STATUS verifyRegistryTable(const UINT8 *table_bytes, UINTN table_bytes_len, const REGISTRY_TABLE_HEADER *table, BOOLEAN *result) {
    EFI_STATUS Status;
    *result = FALSE;

    if (table_bytes_len < sizeof(REGISTRY_TABLE_ENTRY)) {
        return EFI_BUFFER_TOO_SMALL;
    }

    UINT32 num_entries = table->numOfEntries;
    UINTN table_size = sizeof(REGISTRY_TABLE_HEADER) + num_entries * sizeof(REGISTRY_TABLE_ENTRY);

    if (table_bytes_len < table_size) {
        return EFI_BUFFER_TOO_SMALL;
    }

    UINT8 *buf;
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, table_size, (VOID **)&buf);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    CopyMem(buf, table_bytes, table_size);

    for (UINTN i = REGISTRY_TABLE_CRC_OFFSET; i < REGISTRY_TABLE_CRC_OFFSET + 4; i++) {
        buf[i] = 0;
    }

    UINT32 crc = crc32(buf, table_size);

    Print(L"INFO: (config) Registry Table CRC from file: 0x%08X, calculated: 0x%08X\n", table->checksum, crc);

    *result = (crc == table->checksum);

    Status = uefi_call_wrapper(gBS->FreePool, 1, buf);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

BOOLEAN arrays_equal(CHAR8* a, CHAR8* b, UINTN n) {
    for (UINTN i = 0; i < n; i++) {
        if (a[i] != b[i])
            return FALSE;
    }
    return TRUE;
}

EFI_STATUS getConfig(CHAR16* path, ConfigNode** output) {
    VOID* content;
    UINTN contentSize;
    EFI_STATUS Status = loadFile(path, &content, &contentSize);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN pos = 0;
    if (contentSize < pos + sizeof(HEADER)) {
        FreePool(content);
        return EFI_END_OF_FILE;
    }

    HEADER header;
    CopyMem(&header, content + pos, sizeof(HEADER));
    pos += sizeof(HEADER);

    CHAR8 expectedMagic[4] = {'N', 'D', 'R', '\00'};
    if (!arrays_equal(expectedMagic, header.magic, 4)) {
        FreePool(content);
        return EFI_INCOMPATIBLE_VERSION;
    }

    if(!verifyHeader(&header)) {
        FreePool(content);
        return EFI_CRC_ERROR;
    }

    if (contentSize < pos + sizeof(REGISTRY_TABLE_HEADER)) {
        FreePool(content);
        return EFI_END_OF_FILE;
    }

    REGISTRY_TABLE_HEADER regHeader;
    CopyMem(&regHeader, content + pos, sizeof(REGISTRY_TABLE_HEADER));
    UINTN regHeaderStart = pos;
    pos += sizeof(REGISTRY_TABLE_HEADER);

    UINTN entryCount = regHeader.numOfEntries;
    Print(L"INFO: (config) Found %ld entries\n", entryCount);
    REGISTRY_TABLE_ENTRY* entriesPtr = (REGISTRY_TABLE_ENTRY*)AllocatePool(sizeof(REGISTRY_TABLE_ENTRY) * entryCount);
    Print(L"INFO: (config) Allocated %ld bytes for entriesPtr\n", sizeof(REGISTRY_TABLE_ENTRY) * entryCount);

    for (UINTN i = 0; i < entryCount; i++) {
        if (pos + sizeof(REGISTRY_TABLE_ENTRY) > contentSize) {
            FreePool(content);
            FreePool(entriesPtr);
            return EFI_END_OF_FILE;
        }

        CopyMem(&entriesPtr[i], (REGISTRY_TABLE_ENTRY*)((UINT8*)content + pos), sizeof(REGISTRY_TABLE_ENTRY));
        pos += sizeof(REGISTRY_TABLE_ENTRY);
    }

    BOOLEAN regHeaderCrcCheck = FALSE;
    Status = verifyRegistryTable(content + regHeaderStart, pos - regHeaderStart, &regHeader, &regHeaderCrcCheck);
    if (EFI_ERROR(Status)) {
        FreePool(content);
        FreePool(entriesPtr);
        return Status;
    }

    if (!regHeaderCrcCheck) {
        FreePool(content);
        FreePool(entriesPtr);
        return EFI_CRC_ERROR;
    }

    for (UINTN i = 0; i < entryCount; i++) {
        REGISTRY_TABLE_ENTRY entry = entriesPtr[i];
        Print(L"INFO: (config) Entry %ld id=0x%lX offset=0x%lX rootID=0x%lX\n", i, entry.entryId, entry.entryOffset, entry.rootId);
    }

    REGISTRY_ENTRY* entriesHeaders = (REGISTRY_ENTRY*)AllocatePool(sizeof(REGISTRY_ENTRY) * entryCount);
    for (UINTN i = 0; i < entryCount; i++) {
        REGISTRY_TABLE_ENTRY entry = entriesPtr[i];
        pos = entry.entryOffset;
        if (pos % 16 != 0) {
            Print(L"WARNING: (config) Entry %ld is not 16 bytes aligned: offset=0x%lX\n", i, pos);
        }

        CopyMem(&entriesHeaders[i].entrySize, content + pos, sizeof(UINT64));
        pos += sizeof(UINT64);
        CopyMem(&entriesHeaders[i].entryId, content + pos, sizeof(UINT64));
        pos += sizeof(UINT64);
        CopyMem(&entriesHeaders[i].entryNameSize, content + pos, sizeof(UINT64));
        pos += sizeof(UINT64);
        CopyMem(&entriesHeaders[i].rsvd1, content + pos, sizeof(UINT64));
        pos += sizeof(UINT64);

        if (entriesHeaders[i].entryId != entry.entryId) {
            Print(L"WARNING: (config) Invalid entry ID: entry=%lX, re=%lX\n", entriesHeaders[i].entryId, entry.entryId);
        }

        UINTN nameStart = pos;
        UINTN nameEnd = entriesHeaders[i].entryNameSize + nameStart;
        if (nameEnd > contentSize) {
            Print(L"ERROR: (config) Entry %ld: name out of bounds", i);
            FreePool(content);
            FreePool(entriesPtr);
            FreePool(entriesHeaders);
            return EFI_END_OF_FILE;
        }

        CHAR8* entryName = (CHAR8*)AllocatePool(nameEnd - nameStart);
        CopyMem(entryName, content + nameStart, nameEnd - nameStart);
        entriesHeaders[i].entryName = entryName;
        pos = nameEnd;

        CopyMem(&entriesHeaders[i].entryType, content + pos, sizeof(UINT32));
        pos += sizeof(UINT32);
        CopyMem(&entriesHeaders[i].rsvd2, content + pos, sizeof(UINT32));
        pos += sizeof(UINT32);
        CopyMem(&entriesHeaders[i].flags, content + pos, sizeof(UINT32));
        pos += sizeof(UINT32);

        UINTN dataStart = pos;
        UINTN dataEnd = entry.entryOffset + entriesHeaders[i].entrySize;
        if (dataEnd > contentSize) {
            Print(L"ERROR: (config) Entry %ld: name out of bounds", i);
            FreePool(content);
            FreePool(entriesPtr);
            FreePool(entriesHeaders);
            return EFI_END_OF_FILE;
        }

        UINT8* entryData = (UINT8*)AllocatePool(dataEnd - dataStart);
        CopyMem(entryData, content + dataStart, dataEnd - dataStart);
        entriesHeaders[i].data = entryData;
        pos = dataEnd;

        entriesHeaders[i].dataSize = dataEnd - dataStart;
        Print(L"INFO: (config) Entry %ld: id=0x%lX, flags=0x%X, data_len=%ld\n", i, entriesHeaders[i].entryId, entriesHeaders[i].flags, entriesHeaders[i].dataSize);
    }

    ConfigNode* tree = (ConfigNode*)AllocatePool(sizeof(ConfigNode) * (entryCount + 1));
    Print(L"INFO: (config) Allocated %ld bytes for registry tree\n", sizeof(ConfigNode) * (entryCount + 1));

    CHAR8* rootName = (CHAR8*)"root";
    CHAR8* rootNameBuf = (CHAR8*)AllocatePool(5);
    CopyMem(rootNameBuf, rootName, 5);

    tree[0] = ConfigNodeInit(rootNameBuf, T_NODE, NULL, 0);

    for (UINTN i = 0; i < entryCount; i++) {
        REGISTRY_ENTRY entry = entriesHeaders[i];
        CHAR8* entryName = (CHAR8*)AllocatePool(entry.entryNameSize + sizeof(CHAR8));
        CopyMem(entryName, entry.entryName, entry.entryNameSize);
        entryName[entry.entryNameSize] = (CHAR8)'\0';
        UINT8* entryData = NULL;
        if (entry.dataSize > 0) {
            entryData = (UINT8*)AllocatePool(entry.dataSize);
            CopyMem(entryData, entry.data, entry.dataSize);
        }
        tree[i + 1] = ConfigNodeInit(entryName, entry.entryType, entryData, entry.dataSize);
    }

    for (UINTN i = 0; i < entryCount; i++) {
        REGISTRY_TABLE_ENTRY entry = entriesPtr[i];
        ConfigNode* currentNode = &tree[i + 1];

        if (entry.rootId == 0) {
            ConfigNodeAddChild(&tree[0], currentNode);
            continue;
        }

        UINTN parentIndex = 0;
        for (UINTN j = 0; j < entryCount; j++) {
            if (entriesPtr[j].entryId == entry.rootId) {
                parentIndex = j + 1;
                break;
            }
        }
        if (parentIndex != 0) {
            ConfigNodeAddChild(&tree[parentIndex], currentNode);
        } else {
            ConfigNodeAddChild(&tree[0], currentNode);
        }
    }

    FreePool(content);
    FreePool(entriesPtr);
    FreePool(entriesHeaders);

    *output = tree;
    return EFI_SUCCESS;
}