#include "modstd.h"
#include <stdint.h>

REGISTER_MODULE("elf")

#define VFS_OPEN    3
#define VFS_READ    4
#define VFS_CLOSE   6

#define ELF_LOAD    1
#define EXECV       2

#define PAGE_SIZE   4096
#define DEFAULT_BASE 0x1000000ULL

// ELF64 constants
#define EI_MAG       0
#define EI_CLASS     4
#define EI_DATA      5
#define ELFCLASS64   2
#define ELFDATA2LSB  1
#define ET_EXEC      2
#define ET_DYN       3
#define EM_X86_64    62
#define PT_LOAD      1

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

static uint64_t vfs_pid = 0;

// Used by EXECV – must be static so fork()d child sees the same values
static uint64_t exec_entry = 0;
static uint64_t exec_sp = 0;

#define MAX_SHM_CACHE 8
static unsigned long long shm_handles[MAX_SHM_CACHE];
static unsigned long long shm_vaddrs[MAX_SHM_CACHE];
static int shm_cache_count = 0;

static int vfs_send(uint64_t* msg, uint64_t* reply) {
    if (!vfs_pid) return -1;
    mod_send(vfs_pid, msg);
    unsigned long long snd;
    do { snd = mod_recv_from(vfs_pid, reply); } while (snd != vfs_pid);
    return (reply[0] == 0) ? 0 : -1;
}

// Read from open VFS file into a local buffer (up to PAGE_SIZE bytes).
// Returns number of bytes read, or -1 on error.
static int file_read(uint64_t fd, unsigned long long shm,
                     uint64_t offset, uint32_t size) {
    if (size > PAGE_SIZE) size = PAGE_SIZE;
    uint64_t m[8] = {VFS_READ, fd, offset, shm, size};
    uint64_t r[8];
    if (vfs_send(m, r) != 0) return -1;
    return (int)r[1];
}

// Load an ELF file: parse headers, allocate pages for PT_LOAD segments,
// read segment data. Returns 0 on success with *entry and *first_seg set.
// Returns -1 on error.
static int load_elf(uint64_t fd, uint64_t file_size, unsigned long long shm,
                    uint8_t* buf, uint64_t* out_entry) {
    // Read ELF header
    if (file_read(fd, shm, 0, sizeof(Elf64_Ehdr)) < (int)sizeof(Elf64_Ehdr))
        return -1;

    Elf64_Ehdr* eh = (Elf64_Ehdr*)buf;
    if (eh->e_ident[EI_MAG+0] != 0x7F || eh->e_ident[EI_MAG+1] != 'E' ||
        eh->e_ident[EI_MAG+2] != 'L'  || eh->e_ident[EI_MAG+3] != 'F' ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) ||
        eh->e_machine != EM_X86_64)
        return -1;

    uint64_t phoff = eh->e_phoff;
    uint16_t phnum = eh->e_phnum;
    uint16_t phentsize = eh->e_phentsize;

    if (phnum == 0 || phentsize != sizeof(Elf64_Phdr) ||
        phoff + phnum * phentsize > file_size)
        return -1;

    // Determine load offset: if PIE or base VMA is very low, use DEFAULT_BASE
    uint64_t base_vaddr = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t elf_entry = eh->e_entry;
    uint16_t elf_type = eh->e_type;
    for (int i = 0; i < phnum; i++) {
        if (file_read(fd, shm, phoff + i * phentsize, sizeof(Elf64_Phdr))
            < (int)sizeof(Elf64_Phdr)) return -1;
        Elf64_Phdr* ph = (Elf64_Phdr*)buf;
        if (ph->p_type == PT_LOAD && ph->p_vaddr < base_vaddr)
            base_vaddr = ph->p_vaddr;
    }

    uint64_t load_off = (base_vaddr < 0x100000ULL || elf_type == ET_DYN)
                        ? DEFAULT_BASE : 0;
    *out_entry = elf_entry + load_off;

    // Load PT_LOAD segments
    for (int i = 0; i < phnum; i++) {
        if (file_read(fd, shm, phoff + i * phentsize, sizeof(Elf64_Phdr))
            < (int)sizeof(Elf64_Phdr)) return -1;
        Elf64_Phdr* ph = (Elf64_Phdr*)buf;
        if (ph->p_type != PT_LOAD) continue;

        uint64_t vaddr = ph->p_vaddr + load_off;
        uint64_t memsz = ph->p_memsz;
        uint64_t filesz = ph->p_filesz;
        uint64_t offset = ph->p_offset;

        uint64_t page_off = vaddr & 0xFFF;
        uint64_t page_vaddr = vaddr & ~0xFFFULL;
        uint64_t total = memsz + page_off;
        uint64_t pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
        if (pages == 0) continue;

        // Allocate pages at the target vaddr
        unsigned long long av = mod_syscall(SYSCALL_ALLOC_PAGES, pages, page_vaddr, 0);
        if (av == 0 || av == (unsigned long long)-1) return -1;

        // Read file data
        for (uint64_t po = 0; po < filesz; ) {
            uint64_t chunk = filesz - po;
            if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;
            int n = file_read(fd, shm, offset + po, chunk);
            if (n < (int)chunk) return -1;
            uint8_t* dst = (uint8_t*)(page_vaddr + po);
            for (uint64_t ci = 0; ci < chunk; ci++) dst[ci] = buf[ci];
            po += chunk;
        }

        // Zero BSS
        if (memsz > filesz) {
            uint64_t bss_start = page_vaddr + filesz;
            uint64_t bss_end = page_vaddr + total;
            for (uint64_t z = bss_start; z < bss_end; z++)
                *(volatile uint8_t*)z = 0;
        }
    }

    return 0;
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* a = (uint64_t*)msg;
    uint64_t reply[8] = {0};

    switch (a[0]) {

    case ELF_LOAD: {
        const char* path = (const char*)(msg + 8);
        uint64_t fd = (uint64_t)-1;
        unsigned long long shm = (unsigned long long)-1;
        int err = 0;
        uint64_t entry = 0;

        uint64_t om[8] = {VFS_OPEN, 0, 0, 0, 0, 0, 0, 0};
        int pi = 0;
        while (path[pi] && pi < 55) { ((char*)om)[8 + pi] = path[pi]; pi++; }
        ((char*)om)[8 + pi] = '\0';
        uint64_t orr[8];
        if (vfs_send(om, orr) != 0) { err = 1; goto elf_load_cleanup; }
        fd = orr[1];
        uint64_t file_size = orr[2];

        if (file_size < sizeof(Elf64_Ehdr)) { err = 1; goto elf_load_cleanup; }

        shm = shm_create(1, SHM_ALL_RIGHTS);
        if (shm == (unsigned long long)-1) { err = 1; goto elf_load_cleanup; }
        unsigned long long shm_vaddr = shm_attach(shm);
        if (shm_vaddr == (unsigned long long)-1) { err = 1; goto elf_load_cleanup; }

        if (load_elf(fd, file_size, shm, (uint8_t*)shm_vaddr, &entry) != 0)
            err = 1;

        if (err)
            reply[0] = -1;
        else {
            reply[0] = 0;
            reply[1] = entry;
        }

    elf_load_cleanup:
        if (shm != (unsigned long long)-1) shm_detach(shm);
        if (fd != (uint64_t)-1) {
            uint64_t cm[8] = {VFS_CLOSE, fd, 0, 0, 0, 0, 0, 0};
            uint64_t cr[8];
            vfs_send(cm, cr);
        }
        break;
    }

    case EXECV: {
        const char* path = (const char*)(msg + 8);
        uint64_t fd = (uint64_t)-1;
        unsigned long long shm = (unsigned long long)-1;
        uint64_t entry = 0;

        // Open file
        uint64_t om[8] = {VFS_OPEN, 0, 0, 0, 0, 0, 0, 0};
        int pi = 0;
        while (path[pi] && pi < 55) { ((char*)om)[8 + pi] = path[pi]; pi++; }
        ((char*)om)[8 + pi] = '\0';
        uint64_t orr[8];
        if (vfs_send(om, orr) != 0) { debug_puts("ELF: VFS_OPEN fail\n"); reply[0] = -1; goto execv_cleanup; }
        fd = orr[1];
        uint64_t file_size = orr[2];
        debug_puts("ELF: opened fd="); debug_putu(fd); debug_puts(" size="); debug_putu(file_size); debug_puts("\n");

        if (file_size < sizeof(Elf64_Ehdr)) { debug_puts("ELF: file too small\n"); reply[0] = -1; goto execv_cleanup; }

        shm = shm_create(1, SHM_ALL_RIGHTS);
        if (shm == (unsigned long long)-1) { debug_puts("ELF: shm_create fail\n"); reply[0] = -1; goto execv_cleanup; }
        unsigned long long shm_vaddr = shm_attach(shm);
        if (shm_vaddr == (unsigned long long)-1) { debug_puts("ELF: shm_attach fail\n"); reply[0] = -1; goto execv_cleanup; }

        debug_puts("ELF: loading ELF\n");
        // Load ELF (allocates segment pages in our address space)
        if (load_elf(fd, file_size, shm, (uint8_t*)shm_vaddr, &entry) != 0)
            { debug_puts("ELF: load_elf fail\n"); reply[0] = -1; goto execv_cleanup; }

        debug_puts("ELF: allocating stack\n");
        // Allocate stack (4 pages) for the new process
        unsigned long long stack = mod_syscall(SYSCALL_ALLOC_PAGES, 4, 0, 0);
        if (stack == 0 || stack == (unsigned long long)-1) { debug_puts("ELF: stack alloc fail\n"); reply[0] = -1; goto execv_cleanup; }
        uint64_t stack_top = stack + 4 * PAGE_SIZE;

        // Shm no longer needed
        shm_detach(shm);
        shm = (unsigned long long)-1;

        // Close file
        {
            uint64_t cm[8] = {VFS_CLOSE, fd, 0, 0, 0, 0, 0, 0};
            uint64_t cr[8];
            vfs_send(cm, cr);
        }
        fd = (uint64_t)-1;

        // Fork – child inherits the loaded ELF pages + stack
        exec_entry = entry;
        exec_sp = stack_top;
        debug_puts("ELF: entry="); debug_putu(entry); debug_puts(" sp="); debug_putu(stack_top); debug_puts("\n");
        debug_puts("ELF: forking\n");
        uint64_t child_pid = fork();
        if (child_pid == 0) {
            syscall(SYSCALL_MOD, MOD_CHANGE_PROCESS_NAME, child_pid, "last_path_part\0", 16); // TODO: Get path last part to name process
            syscall(SYSCALL_MOD, MOD_UNREGISTER, 0, 0, 0);

            asm volatile(
                "mov %0, %%rsp;"
                "push %1;"
                "xor %%rbp, %%rbp;"
                "xor %%rax, %%rax;"
                "xor %%rbx, %%rbx;"
                "xor %%rcx, %%rcx;"
                "xor %%rdx, %%rdx;"
                "xor %%rsi, %%rsi;"
                "xor %%rdi, %%rdi;"
                "xor %%r8, %%r8;"
                "xor %%r9, %%r9;"
                "xor %%r10, %%r10;"
                "xor %%r11, %%r11;"
                "xor %%r12, %%r12;"
                "xor %%r13, %%r13;"
                "xor %%r14, %%r14;"
                "xor %%r15, %%r15;"
                "pop %%rax;"
                "jmp *%%rax;"
                : : "r"(exec_sp), "r"(exec_entry));
            __builtin_unreachable();
        }

        debug_puts("ELF: forked child_pid="); debug_putu(child_pid); debug_puts("\n");
        reply[0] = 0;
        reply[1] = child_pid;
        break;

    execv_cleanup:
        debug_puts("ELF: EXECV cleanup\n");
        if (shm != (unsigned long long)-1) shm_detach(shm);
        if (fd != (uint64_t)-1) {
            uint64_t cm[8] = {VFS_CLOSE, fd, 0, 0, 0, 0, 0, 0};
            uint64_t cr[8];
            vfs_send(cm, cr);
        }
        reply[0] = -1;
        break;
    }

    default:
        reply[0] = -1;
    }

    mod_send(sender, reply);
}

void init(void) {
    vfs_pid = find_mod("vfs");
    if (!vfs_pid) {
        debug_puts("ELF: VFS not found\n");
    }
}

void loop(void) {
    if (!vfs_pid) vfs_pid = find_mod("vfs");
    unsigned char msg[64];
    unsigned long long snd = mod_recv(msg);
    if (snd != (unsigned long long)-1)
        handle_ipc(msg, snd);
    else
        asm volatile("pause");
}

