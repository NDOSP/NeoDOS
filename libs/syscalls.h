#ifndef SYSCALLS
#define SYSCALLS

#define SYSCALL_EXIT        1
#define SYSCALL_FORK        2
#define SYSCALL_GETPID      3
#define SYSCALL_SEND        4
#define SYSCALL_RECV        5
#define SYSCALL_ALLOC_PAGES 6
#define SYSCALL_FREE_PAGES  7
#define SYSCALL_SHM            8
#define SYSCALL_RECV_FROM      9
#define SYSCALL_MOD_LIST       12
#define SYSCALL_MOD            20
#define SYSCALL_NDR            21
#define SYSCALL_WRITE       0xFF00000000000001

#define SHM_CREATE  1
#define SHM_ATTACH  2
#define SHM_DETACH  3

#define NDR_SIZE    1
#define NDR_COPY    2

static inline unsigned long long syscall(unsigned long long n,
                                         unsigned long long a1,
                                         unsigned long long a2,
                                         unsigned long long a3,
                                         unsigned long long a4) {
    unsigned long long ret;
    register unsigned long long rax asm("rax") = n;
    register unsigned long long rdi asm("rdi") = a1;
    register unsigned long long rsi asm("rsi") = a2;
    register unsigned long long rdx asm("rdx") = a3;
    register unsigned long long r10 asm("r10") = a4;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10)
                 : "rcx", "r11", "memory");
    return ret;
}

static inline void exit() {
    syscall(SYSCALL_EXIT, 0, 0, 0, 0);
}

static inline unsigned long long send(unsigned long long pid, void* buf) {
    return syscall(SYSCALL_SEND, pid, (unsigned long long)buf, 0, 0);
}

static inline unsigned long long recv(void* buf) {
    return syscall(SYSCALL_RECV, (unsigned long long)buf, 0, 0, 0);
}

static inline unsigned long long shm_create(unsigned long long pages) {
    return syscall(SYSCALL_SHM, SHM_CREATE, pages, 0, 0);
}

static inline unsigned long long shm_attach(unsigned long long handle) {
    return syscall(SYSCALL_SHM, SHM_ATTACH, handle, 0, 0);
}

static inline unsigned long long shm_detach(unsigned long long handle) {
    return syscall(SYSCALL_SHM, SHM_DETACH, handle, 0, 0);
}

#endif // SYSCALLS