#include "video.h"

VideoFramebuffer fbData;

void initVideo(VideoFramebuffer fb) {
    fbData = fb;
}

static inline uint32_t colorToUint32(VideoColor c, int format) {
    switch (format) {
        case VIDEO_BGRA: return ((uint32_t)c.blue) | ((uint32_t)c.green << 8) | ((uint32_t)c.red  << 16);
        case VIDEO_RGBA: return ((uint32_t)c.red) | ((uint32_t)c.green << 8) | ((uint32_t)c.blue   << 16);
    }
    return 0;
}

void putPixel(uint32_t x, uint32_t y, VideoColor color) {
    if (x >= fbData.fbWidth) return;
    if (y >= fbData.fbHeight) return;

    uint32_t* fbPtr = (uint32_t*)fbData.fbPtr;
    uint64_t pos = x + y * fbData.fbWidth;
    fbPtr[pos] = colorToUint32(color, fbData.pixelFormat);
}