#include "modstd.h"
#include "vfs/vfs.h"

REGISTER_MODULE("console")

void init(void) {
    VFSModTable* vfs = (VFSModTable*)get_mod_table("vfs");
    FileDetail file = MODTABLE_CALL(vfs, vfs->create, "T:\\CONHAN");

    // TODO: Create VFS handle for stdout (T:\CONHAN) and put it on screen
}

void loop(void) {
    return;
}