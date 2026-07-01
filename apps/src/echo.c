#include "modlib.h"

// Future user command: echo <text> — print text.
// Loaded on demand by shell; not auto-started.
__attribute__((section(".text.start")))
void _start(void) {
    mod_syscall(SYSCALL_EXIT, 0, 0, 0);
}
