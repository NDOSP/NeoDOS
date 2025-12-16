module bootinfo;

enum VideoType {
    ARGB,
    RGBA,
    ABGR,
    BGRA
}

struct VideoFramebuffer {
    void* fbPtr;
    usz fbSize;
    uint fbWidth;
    uint fbHeight;
    uint fbScanlineBytes;
    VideoType pixelFormat;
}

struct MappingInfo {
    ulong paddr;
    ulong vaddr;
    ulong size;
    uint flags;
}

struct KernelInfo {
    ulong entryPoint;
    MappingInfo[255] segmentMapping;
    char segmentCount;
    MappingInfo bootInfo;
}

struct Rsdp {
    char[8] signature;  
    char checksum;
    char[6] oemId;
    char revision;    
    uint rsdtAddress;
    uint length;
    ulong xsdtAddress;
    char extendedChecksum;
    char[3] reserved;
}

struct MemoryMapEntry {
    ulong address;
    ulong size;
    uint type;
    uint[3] reserved;
}

struct MemoryMap {
    usz numberOfEntries;
    usz mapKey;
    usz descriptorSize;
    usz descriptorVersion;
    MemoryMapEntry[4096 / MemoryMapEntry.sizeof] entries @align(4096);
}

struct BootInfo {
    ulong* pml4;
    ulong stackCount;
    VideoFramebuffer fb;
    KernelInfo kInfo;
    Rsdp* rsdp;
}