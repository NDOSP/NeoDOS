#define SYSCALL_GETPID 3
#define SYSCALL_SEND 4
#define SYSCALL_RECV 5
#define SYSCALL_WRITE 0xFF00000000000001
#define SYSCALL_ALLOC_PAGES 6

static unsigned long long syscall(unsigned long long n,
                                 unsigned long long a1,
                                 unsigned long long a2,
                                 unsigned long long a3) {
    unsigned long long ret;
    register unsigned long long rax asm("rax") = n;
    register unsigned long long rdi asm("rdi") = a1;
    register unsigned long long rsi asm("rsi") = a2;
    register unsigned long long rdx asm("rdx") = a3;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx)
                 : "rcx", "r11", "memory");
    return ret;
}

static void write_str(const char* s) {
    unsigned long long len = 0;
    while (s[len]) len++;
    syscall(SYSCALL_WRITE, (unsigned long long)s, len, 0);
}

static unsigned long long send(unsigned long long pid, void* buf) {
    return syscall(SYSCALL_SEND, pid, (unsigned long long)buf, 0);
}

static unsigned long long recv(void* buf) {
    return syscall(SYSCALL_RECV, (unsigned long long)buf, 0, 0);
}

__attribute__((noreturn))
void _start(void) {
    write_str("INIT: NeoDOS microkernel init started\n");
    
    // Get our PID
    unsigned long long mypid = syscall(SYSCALL_GETPID, 0, 0, 0);
    char pid_str[32];
    // Simple itoa
    unsigned long long tmp = mypid;
    int idx = 0;
    do {
        pid_str[idx++] = '0' + (tmp % 10);
        tmp /= 10;
    } while (tmp > 0 && idx < 31);
    for (int i = 0; i < idx/2; i++) {
        char c = pid_str[i];
        pid_str[i] = pid_str[idx-1-i];
        pid_str[idx-1-i] = c;
    }
    pid_str[idx] = '\0';
    
    write_str("INIT: My PID is ");
    write_str(pid_str);
    write_str("\n");
    
    write_str("INIT: System ready - waiting for messages\n");

    while (1) {
        // Process messages - simple echo server
        unsigned char buf[64];
        unsigned long long sender = recv(buf);
        if (sender != (unsigned long long)-1) {
            write_str("INIT: Got message from ");
            // Simple itoa for sender
            tmp = sender;
            idx = 0;
            do {
                pid_str[idx++] = '0' + (tmp % 10);
                tmp /= 10;
            } while (tmp > 0 && idx < 31);
            for (int i = 0; i < idx/2; i++) {
                char c = pid_str[i];
                pid_str[i] = pid_str[idx-1-i];
                pid_str[idx-1-i] = c;
            }
            pid_str[idx] = '\0';
            write_str(pid_str);
            write_str("\n");
            
            // Echo back
            send(sender, buf);
        }
        asm volatile("pause");
    }
}
