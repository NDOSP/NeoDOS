#ifndef CHECKSUM_BOOT_H
#define CHECKSUM_BOOT_H

#include <stdlib.h>
#include <efi.h>
#include <efilib.h>

static inline uint32_t crc32(const VOID* data, UINTN length) {
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

#endif