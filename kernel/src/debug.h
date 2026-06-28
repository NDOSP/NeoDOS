#ifndef DEBUG_H
#define DEBUG_H

#include "serial.h"

#define DEBUG_INFO(fmt, ...)  serial_printf("INFO: (kernel) " fmt "\n", ##__VA_ARGS__)
#define DEBUG_WARN(fmt, ...)  serial_printf("WARNING: (kernel) " fmt "\n", ##__VA_ARGS__)
#define DEBUG_ERROR(fmt, ...) serial_printf("ERROR: (kernel) " fmt "\n", ##__VA_ARGS__)

#endif
