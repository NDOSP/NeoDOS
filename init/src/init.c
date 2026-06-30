#include "modlib.h"

__attribute__((noreturn, section(".text.start")))
void _start(void) {
    debug_puts("INIT: NeoDOS init started\n");
    
    unsigned long long mypid = mod_syscall(SYSCALL_GETPID, 0, 0, 0);
    char pid_str[32];
    mod_itoa(mypid, pid_str, 32);
    
    debug_puts("INIT: My PID is ");
    debug_puts(pid_str);
    debug_puts("\n");
    
    debug_puts("INIT: System ready - waiting for messages\n");

    while (1) {
        unsigned char buf[64];
        unsigned long long sender = mod_recv(buf);
        if (sender != (unsigned long long)-1) {
            debug_puts("INIT: Got message from ");
            mod_itoa(sender, pid_str, 32);
            debug_puts(pid_str);
            debug_puts("\n");
            mod_send(sender, buf);
        }
        asm volatile("pause");
    }
}
