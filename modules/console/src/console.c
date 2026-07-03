#include "modstd.h"
#include "vfs/vfs.h"

REGISTER_MODULE("console")

void init(void) {
    VFSModTable* vfs = (VFSModTable*)get_mod_table("vfs");
    debug_puts("console VFS PID: ");
    debug_putu(vfs->pid);
    debug_puts("\n");

    MODTABLE_CALL(vfs, vfs->open, "test");

    // TODO: Create VFS handle for stdout (T:\CONHAN) and put it on screen
}

void loop(void) {
    return;
}