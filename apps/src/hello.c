#include "modlib.h"

__attribute__((section(".text.start")))
void _start(void) {
    debug_puts("Hello from NeoDOS!\n");
    mod_syscall(SYSCALL_EXIT, 0, 0, 0);
}
