#include "modlib.h"

__attribute__((section(".text.start")))
void _start(void) {
    unsigned long long sz = ndr_size();
    if (sz == (unsigned long long)-1) {
        debug_puts("NDR: no data\n");
        mod_syscall(SYSCALL_EXIT, 0, 0, 0);
    }

    debug_puts("NDR size: ");
    debug_putu(sz);
    debug_puts(" bytes\n");

    unsigned long long pages = (sz + 4095) / 4096;
    unsigned long long buf = mod_syscall(SYSCALL_ALLOC_PAGES, pages, 0, 0);
    if (buf == 0 || buf == (unsigned long long)-1) {
        debug_puts("alloc failed\n");
        mod_syscall(SYSCALL_EXIT, 0, 0, 0);
    }

    unsigned long long copied = ndr_copy((void*)buf, 0, sz);
    if (copied == (unsigned long long)-1) {
        debug_puts("copy failed\n");
        mod_syscall(SYSCALL_EXIT, 0, 0, 0);
    }

    debug_puts("copied ");
    debug_putu(copied);
    debug_puts(" bytes\n");

    // Hex dump first 128 bytes
    unsigned char* d = (unsigned char*)buf;
    unsigned long long max = (copied < 128) ? copied : 128;
    for (unsigned long long i = 0; i < max; i++) {
        char hex[3] = {0};
        unsigned char nib0 = (d[i] >> 4) & 0xF;
        unsigned char nib1 = d[i] & 0xF;
        hex[0] = nib0 < 10 ? '0' + nib0 : 'A' + nib0 - 10;
        hex[1] = nib1 < 10 ? '0' + nib1 : 'A' + nib1 - 10;
        debug_puts(hex);
        if ((i & 15) == 15) debug_puts("\n");
        else if ((i & 7) == 7) debug_puts("  ");
        else debug_puts(" ");
    }
    if (max & 15) debug_puts("\n");

    mod_syscall(SYSCALL_EXIT, 0, 0, 0);
}
