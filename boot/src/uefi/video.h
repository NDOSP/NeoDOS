#ifndef VIDEO_BOOT_H
#define VIDEO_BOOT_H

#include <efi.h>
#include <efilib.h>

typedef struct {
    UINT16 width;
    UINT16 height;
} VIDEO_RESOLUTION;

typedef struct {
    EFI_HANDLE handle;
    VIDEO_RESOLUTION resolution;
} VIDEO_INFO;

typedef enum {
    ARGB,
    RGBA,
    ABGR,
    BGRA
} VIDEO_TYPE;

typedef struct {
    VOID* fbPtr;
    UINTN fbSize;
    UINT32 fbWidth;
    UINT32 fbHeight;
    UINT32 fbScanlineBytes;
    VIDEO_TYPE pixelFormat;
} VIDEO_FRAMEBUFFER;

EFI_STATUS getPreferredResolution(OUT VIDEO_INFO* info);
EFI_STATUS setVideoMode(VIDEO_INFO vInfo, VIDEO_FRAMEBUFFER* fb);

#endif // VIDEO_BOOT_H