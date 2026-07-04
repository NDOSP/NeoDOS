#ifndef STD
#define STD

#define PAGES(bytes) ((bytes + PAGE_SIZE - 1) / PAGE_SIZE)
#define NULL ((void*)0)

#include "syscalls.h"
#include "string.h"
#include "mods.h"

#endif // STD