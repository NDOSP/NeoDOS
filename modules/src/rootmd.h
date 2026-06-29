#ifndef ROOTMD_H
#define ROOTMD_H

#include "module.h"
#include <stdint.h>

#define ROOTMD_PID 1

typedef enum {
    ROOTMD_REGISTER = 1,
    ROOTMD_UNREGISTER = 2,
    ROOTMD_LIST = 3
} RootMdCommand;

typedef struct {
    uint64_t module_pid;
    char name[MOD_NAME_MAX];
} ModuleRegistration;

void rootmd_main();

#endif
