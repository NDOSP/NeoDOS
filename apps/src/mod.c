#include "modlib.h"

// mod – future task loader for NeoDOS.
// When shell + runtime loader exist:
//   mod              → list running tasks
//   mod load <path>  → load a .mod binary from disk
//   mod kill <pid>   → terminate a task
//
// For now: placeholder that exits immediately.
// Implementation requires kernel SYSCALL_MOD subfunction
// for spawning a task from memory.
__attribute__((section(".text.start")))
void _start(void) {
    mod_syscall(SYSCALL_EXIT, 0, 0, 0);
}
