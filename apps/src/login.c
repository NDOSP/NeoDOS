#include "modlib.h"

// Future: login prompt, credential check, launch shell.
__attribute__((section(".text.start")))
void _start(void) {
    mod_syscall(SYSCALL_EXIT, 0, 0, 0);
}
