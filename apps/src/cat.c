#include "modlib.h"

// Future user command: cat <path> — dump file contents to console.
// Loaded on demand by shell; not auto-started.
__attribute__((section(".text.start")))
void _start(void) {
    mod_syscall(SYSCALL_EXIT, 0, 0, 0);
}
