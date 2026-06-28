#include "reg.h"
#include "../string.h"
#include "../memory/memutils.h"

static struct {
    const void* rawData;
    uint64_t rawSize;
    uint64_t entryCount;
    RegEntry entries[REG_MAX_ENTRIES];
} regState;

void regInit(const void* data, uint64_t size) {
    regState.rawData = data;
    regState.rawSize = size;
    regState.entryCount = 0;

    if (!data || size < 28) return;

    // Header: magic(4) + version(4) + rsvd1(4) + checksum(4) + totalSize(8) + rsvd(8) = 32
    const uint8_t* p = (const uint8_t*)data;
    if (p[0] != 'N' || p[1] != 'D' || p[2] != 'R' || p[3] != 0) return;

    uint64_t pos = 32;
    if (pos + 16 > size) return;

    // Registry table header
    uint64_t numOfEntries;
    memcpy(&numOfEntries, p + pos, 8); pos += 8;
    pos += 4; // rsvd
    pos += 4; // checksum

    if (numOfEntries > REG_MAX_ENTRIES) numOfEntries = REG_MAX_ENTRIES;

    // Read entry table
    for (uint64_t i = 0; i < numOfEntries; i++) {
        if (pos + 32 > size) break;

        uint64_t entryId, entryOffset, rootId;
        memcpy(&entryId, p + pos, 8); pos += 8;
        memcpy(&entryOffset, p + pos, 8); pos += 8;
        memcpy(&rootId, p + pos, 8); pos += 8;
        pos += 8; // rsvd

        // Entry header: entrySize(8) + entryId(8) + entryNameSize(8) + rsvd1(8) = 32 minimum
        if (entryOffset + 32 > size) break;

        // Read entry header at entryOffset
        uint64_t ePos = entryOffset;
        uint64_t entrySize, entryNameSize;
        memcpy(&entrySize, p + ePos, 8); ePos += 8;
        // entryId check
        ePos += 8; // entryId
        memcpy(&entryNameSize, p + ePos, 8); ePos += 8;
        ePos += 8; // rsvd1

        if (entryOffset + entrySize > size) break;

        // Name
        const char* name = (const char*)(p + ePos);
        ePos += entryNameSize;

        // Type
        uint32_t entryType;
        memcpy(&entryType, p + ePos, 4); ePos += 4;
        ePos += 4; // rsvd2
        ePos += 4; // flags

        // Data
        const void* entryData = p + ePos;
        uint64_t dataSize = entryOffset + entrySize - ePos;

        RegEntry* e = &regState.entries[regState.entryCount++];
        e->id = entryId;
        e->rootId = rootId;
        e->name = name;
        e->nameLen = entryNameSize;
        e->type = (RegType)entryType;
        e->data = entryData;
        e->dataSize = dataSize;

    }
}

static int nameMatch(const char* a, uint64_t aLen, const char* b, uint64_t bLen) {
    if (aLen != bLen) return 0;
    for (uint64_t i = 0; i < aLen; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static int findEntryByPath(const char* path, RegEntry** out) {
    if (!path || !out || regState.entryCount == 0) return 0;

    uint64_t currentId = 0;
    const char* start = path;
    const char* p = path;

    while (1) {
        if (*p == '/' || *p == '\0') {
            uint64_t len = p - start;
            if (len > 0) {
                int found = 0;
                for (uint64_t i = 0; i < regState.entryCount; i++) {
                    RegEntry* e = &regState.entries[i];
                    if (e->rootId == currentId && nameMatch(e->name, e->nameLen, start, len)) {
                        currentId = e->id;
                        *out = e;
                        found = 1;
                        break;
                    }
                }
                if (!found) return 0;
            }
            if (*p == '\0') return 1;
            start = p + 1;
        }
        p++;
    }
}

uint64_t regReadU64(const char* path, uint64_t def) {
    RegEntry* e;
    if (!findEntryByPath(path, &e)) return def;
    if (e->type != REG_NUM || e->dataSize < 8) return def;
    uint64_t val;
    memcpy(&val, e->data, 8);
    return val;
}

const char* regReadStr8(const char* path, const char* def) {
    RegEntry* e;
    if (!findEntryByPath(path, &e)) return def;
    if (e->type != REG_UTF8) return def;
    return (const char*)e->data;
}
