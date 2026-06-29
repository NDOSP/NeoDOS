#define SYSCALL_GETPID 3
#define SYSCALL_SEND 4
#define SYSCALL_RECV 5
#define SYSCALL_WRITE 0xFF00000000000001
#define SYSCALL_ALLOC_PAGES 6
#define SYSCALL_FREE_PAGES 7

// Module management service (rootmd)
// Provides module loading and lifecycle management in user space

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
                 : "r"(rax), "r"(di), "r"(si), "r"(dx)
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

static void* alloc_pages(unsigned long long n) {
    return (void*)syscall(SYSCALL_ALLOC_PAGES, n, 0, 0);
}

static void free_pages(void* addr, unsigned long long n) {
    syscall(SYSCALL_FREE_PAGES, (unsigned long long)addr, n, 0);
}

static unsigned long long getpid(void) {
    return syscall(SYSCALL_GETPID, 0, 0, 0);
}

// Simple string functions
static int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static void memcpy(void* dst, const void* src, unsigned long long n) {
    for (unsigned long long i = 0; i < n; i++)
        ((unsigned char*)dst)[i] = ((const unsigned char*)src)[i];
}

static void memset(void* dst, int v, unsigned long long n) {
    for (unsigned long long i = 0; i < n; i++)
        ((unsigned char*)dst)[i] = (unsigned char)v;
}

// Simple itoa for debugging
static void itoa(unsigned long long n, char* buf) {
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return;
    }
    while (n > 0 && i < 20) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    for (int j = 0; j < i/2; j++) {
        char c = buf[j];
        buf[j] = buf[i-1-j];
        buf[i-1-j] = c;
    }
    buf[i] = '\0';
}

// ELF loading constants
#define ELF_MAGIC 0x464C457F
#define ET_REL 1
#define SHT_PROGBITS 1
#define SHT_NOBITS 8
#define SHT_RELA 4
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHF_ALLOC 2
#define SHF_EXECINSTR 4
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_32 10
#define R_X86_64_32S 11

// Module state
#define MOD_STATE_STOPPED 0
#define MOD_STATE_RUNNING 1
#define MOD_STATE_LOADED 2

#define MAX_MODULES 16

struct Module {
    unsigned long long pid;
    unsigned long long base;
    unsigned long long pages;
    char name[32];
    unsigned long long state;
};

static struct Module modules[MAX_MODULES];
static unsigned long long module_count = 0;

// Find module by name
static struct Module* find_module(const char* name) {
    for (unsigned long long i = 0; i < module_count; i++) {
        if (strcmp(modules[i].name, name) == 0) {
            return &modules[i];
        }
    }
    return NULL;
}

// Find free module slot
static struct Module* find_free_slot(void) {
    for (unsigned long long i = 0; i < MAX_MODULES; i++) {
        if (modules[i].state == MOD_STATE_STOPPED) {
            return &modules[i];
        }
    }
    return NULL;
}

// Load ELF module
static unsigned long long load_elf_module(unsigned char* elf, unsigned long long size, const char* name) {
    // Check ELF magic
    if (*(unsigned int*)elf != ELF_MAGIC) {
        write_str("ROOTMD: Invalid ELF magic\n");
        return -1;
    }
    
    if (elf[4] != 2) { // 64-bit
        write_str("ROOTMD: Not 64-bit ELF\n");
        return -1;
    }
    
    if (*(unsigned short*)(elf + 16) != ET_REL) { // Relocatable
        write_str("ROOTMD: Not relocatable ELF\n");
        return -1;
    }

    unsigned long long e_shoff = *(unsigned long long*)(elf + 40);
    unsigned short e_shentsize = *(unsigned short*)(elf + 58);
    unsigned short e_shnum = *(unsigned short*)(elf + 60);
    unsigned short e_shstrndx = *(unsigned short*)(elf + 62);

    unsigned char* shstrtab = elf + *(unsigned long long*)(elf + e_shoff + e_shstrndx * e_shentsize + 24);

    unsigned long long symtab_off = 0, symtab_size = 0, symtab_entsize = 0;
    unsigned long long strtab_off = 0, strtab_size = 0;

    unsigned long long sec_size[32], sec_offset[32], sec_flags[32], sec_type[32];
    unsigned long long sec_addr[32];
    int sec_loaded[32] = {0};
    int exec_list[32], non_exec_list[32], bss_list[32];
    int exec_count = 0, non_exec_count = 0, bss_count = 0;

    unsigned long long rela_off[32], rela_size[32], rela_target[32];
    int rela_count = 0;

    // Classify sections
    for (unsigned short i = 0; i < e_shnum; i++) {
        unsigned char* shdr = elf + e_shoff + i * e_shentsize;
        unsigned int sh_type = *(unsigned int*)(shdr + 4);
        unsigned long long sh_flags = *(unsigned long long*)(shdr + 8);
        unsigned long long sh_offset = *(unsigned long long*)(shdr + 24);
        unsigned long long sh_size = *(unsigned long long*)(shdr + 32);
        unsigned int sh_name = *(unsigned int*)shdr;
        char* sn = (char*)(shstrtab + sh_name);

        sec_type[i] = sh_type;
        sec_flags[i] = sh_flags;
        sec_offset[i] = sh_offset;
        sec_size[i] = sh_size;

        int bad_name = (sn[0]=='.'&&sn[1]=='e'&&sn[2]=='h') ||
                       (sn[0]=='.'&&sn[1]=='n'&&sn[2]=='o'&&sn[3]=='t'&&sn[4]=='e') ||
                       (sn[0]=='.'&&sn[1]=='c'&&sn[2]=='o'&&sn[3]=='m'&&sn[4]=='m');

        if (sh_type == SHT_PROGBITS && (sh_flags & SHF_ALLOC) && !bad_name && sh_size > 0) {
            if (sh_flags & SHF_EXECINSTR)
                exec_list[exec_count++] = i;
            else
                non_exec_list[non_exec_count++] = i;
        } else if (sh_type == SHT_NOBITS && (sh_flags & SHF_ALLOC) && !bad_name) {
            bss_list[bss_count++] = i;
        } else if (sh_type == SHT_RELA) {
            rela_off[rela_count] = sh_offset;
            rela_size[rela_count] = sh_size;
            rela_target[rela_count] = *(unsigned int*)(shdr + 44); // sh_info
            rela_count++;
        } else if (sh_type == SHT_SYMTAB) {
            symtab_off = sh_offset; symtab_size = sh_size;
            symtab_entsize = *(unsigned long long*)(shdr + 56);
        } else if (sh_type == SHT_STRTAB && i != e_shstrndx) {
            strtab_off = sh_offset; strtab_size = sh_size;
        }
    }

    if (exec_count == 0) {
        write_str("ROOTMD: No exec sections\n");
        return -1;
    }

    // Compute layout
    unsigned long long addr = 0;
    for (int j = 0; j < exec_count; j++) {
        int idx = exec_list[j];
        sec_addr[idx] = addr;
        sec_loaded[idx] = 1;
        addr += sec_size[idx];
    }
    for (int j = 0; j < non_exec_count; j++) {
        int idx = non_exec_list[j];
        sec_addr[idx] = addr;
        sec_loaded[idx] = 1;
        addr += sec_size[idx];
    }
    for (int j = 0; j < bss_count; j++) {
        int idx = bss_list[j];
        sec_addr[idx] = addr;
        sec_loaded[idx] = 1;
        addr += sec_size[idx];
    }

    // Allocate pages
    unsigned long long pages_needed = (addr + 4095) / 4096;
    unsigned long long text_base = (unsigned long long)alloc_pages(pages_needed);
    if (text_base == (unsigned long long)-1) {
        write_str("ROOTMD: Failed to alloc pages\n");
        return -1;
    }
    memset((void*)text_base, 0, pages_needed * 4096);

    // Relocate section addresses
    for (int j = 0; j < exec_count; j++) {
        int idx = exec_list[j];
        sec_addr[idx] += text_base;
    }
    for (int j = 0; j < non_exec_count; j++) {
        int idx = non_exec_list[j];
        sec_addr[idx] += text_base;
    }
    for (int j = 0; j < bss_count; j++) {
        int idx = bss_list[j];
        sec_addr[idx] += text_base;
    }

    // Copy exec sections
    for (int j = 0; j < exec_count; j++) {
        int idx = exec_list[j];
        memcpy((void*)sec_addr[idx], elf + sec_offset[idx], sec_size[idx]);
    }

    // Copy non-exec sections
    for (int j = 0; j < non_exec_count; j++) {
        int idx = non_exec_list[j];
        if (sec_size[idx] > 0)
            memcpy((void*)sec_addr[idx], elf + sec_offset[idx], sec_size[idx]);
    }

    // Zero BSS
    for (int j = 0; j < bss_count; j++) {
        int idx = bss_list[j];
        memset((void*)sec_addr[idx], 0, sec_size[idx]);
    }

    // Apply relocations
    for (int ri = 0; ri < rela_count; ri++) {
        unsigned long long target_sec = rela_target[ri];
        if (target_sec >= e_shnum || !sec_loaded[target_sec]) continue;
        unsigned long long target_base = sec_addr[target_sec];
        unsigned long long num_entries = rela_size[ri] / 24;

        for (unsigned long long ei = 0; ei < num_entries; ei++) {
            unsigned char* rela = elf + rela_off[ri] + ei * 24;
            unsigned long long r_offset = *(unsigned long long*)(rela + 0);
            unsigned long long r_info = *(unsigned long long*)(rela + 8);
            long long r_addend = *(long long*)(rela + 16);
            unsigned long long sym_idx = r_info >> 32;
            unsigned long long r_type = r_info & 0xFFFFFFFF;

            unsigned long long patch = target_base + r_offset;

            unsigned long long S = 0;
            int sym_ok = 0;
            if (sym_idx > 0 && symtab_size > 0) {
                unsigned char* sym = elf + symtab_off + sym_idx * symtab_entsize;
                unsigned long long st_value = *(unsigned long long*)(sym + 8);
                unsigned short st_shndx = *(unsigned short*)(sym + 6);
                if (st_shndx == 0xFFF1) {
                    S = st_value;
                    sym_ok = 1;
                } else if (st_shndx < e_shnum && sec_loaded[st_shndx]) {
                    S = sec_addr[st_shndx] + st_value;
                    sym_ok = 1;
                }
            }
            if (!sym_ok) continue;

            if (r_type == R_X86_64_32 || r_type == R_X86_64_32S) {
                *(unsigned int*)patch = (unsigned int)(S + r_addend);
            } else if (r_type == R_X86_64_64) {
                *(unsigned long long*)patch = S + r_addend;
            } else if (r_type == R_X86_64_PC32) {
                *(unsigned int*)patch = (unsigned int)(S + r_addend - patch);
            }
        }
    }

    // Find entry symbol
    unsigned long long entry = text_base;
    if (symtab_size > 0 && strtab_size > 0) {
        unsigned long long num_sym = symtab_size / symtab_entsize;
        for (unsigned long long si = 0; si < num_sym; si++) {
            unsigned char* sym = elf + symtab_off + si * symtab_entsize;
            unsigned int st_name = *(unsigned int*)(sym + 0);
            unsigned short st_shndx = *(unsigned short*)(sym + 6);
            unsigned long long st_value = *(unsigned long long*)(sym + 8);
            char* sym_name = (char*)(elf + strtab_off + st_name);
            if (st_shndx != 0 && strcmp(sym_name, "_entry") == 0) {
                if (st_shndx < e_shnum && sec_loaded[st_shndx])
                    entry = sec_addr[st_shndx] + st_value;
                break;
            }
        }
    }

    // Allocate stack
    unsigned long long stack_pages = 4;
    unsigned long long stack = (unsigned long long)alloc_pages(stack_pages);
    if (stack == (unsigned long long)-1) {
        free_pages((void*)text_base, pages_needed);
        write_str("ROOTMD: Failed to alloc stack\n");
        return -1;
    }
    unsigned long long rsp = stack + stack_pages * 4096;

    // Create task - for now, we just return the entry point
    // In a full implementation, we'd create a task via syscall
    // But for now, we'll use a simple approach
    
    struct Module* mod = find_free_slot();
    if (!mod) {
        free_pages((void*)text_base, pages_needed);
        free_pages((void*)stack, stack_pages);
        return -1;
    }
    
    // Store module info
    strcpy(mod->name, name);
    mod->base = text_base;
    mod->pages = pages_needed + stack_pages;
    mod->state = MOD_STATE_LOADED;
    
    // Create task via kernel - we need to send a message to init
    // For now, just return the entry point
    // The caller will create the task
    
    char pid_str[32];
    itoa(entry, pid_str);
    write_str("ROOTMD: Loaded module '");
    write_str(name);
    write_str("' at 0x");
    write_str(pid_str);
    write_str("\n");
    
    return entry;
}

// Command: Load module
// Message format: [cmd=1, data_ptr, size, name_ptr]
static void cmd_load(unsigned long long* msg) {
    unsigned long long data_ptr = msg[1];
    unsigned long long size = msg[2];
    char* name = (char*)(msg + 3); // Name starts at byte 24
    
    unsigned long long entry = load_elf_module((unsigned char*)data_ptr, size, name);
    
    // Response: [result, entry_point]
    unsigned long long resp[4] = {0};
    resp[0] = (entry != (unsigned long long)-1) ? 0 : -1;
    resp[1] = entry;
    
    unsigned long long sender = msg[0]; // First element is sender PID
    send(sender, resp);
}

// Command: List modules
// Message format: [cmd=2]
static void cmd_list(unsigned long long sender) {
    unsigned long long resp[8];
    resp[0] = module_count;
    
    for (int i = 0; i < (int)module_count && i < 4; i++) {
        memcpy((char*)(resp + 1 + i), modules[i].name, 8);
    }
    
    send(sender, resp);
}

// Command: Start module
// Message format: [cmd=3, module_name_ptr]
static void cmd_start(unsigned long long* msg) {
    char* name = (char*)(msg + 2);
    struct Module* mod = find_module(name);
    
    unsigned long long resp[4] = {0};
    
    if (!mod) {
        resp[0] = -1; // Not found
    } else if (mod->state == MOD_STATE_RUNNING) {
        resp[0] = -2; // Already running
    } else {
        // For now, just mark as running
        mod->state = MOD_STATE_RUNNING;
        resp[0] = 0; // Success
    }
    
    send(msg[0], resp); // msg[0] is sender PID
}

__attribute__((noreturn))
void _start(void) {
    // Initialize module list
    for (int i = 0; i < MAX_MODULES; i++) {
        modules[i].state = MOD_STATE_STOPPED;
    }
    module_count = 0;
    
    write_str("ROOTMD: Module management service started\n");
    
    while (1) {
        unsigned long long msg[8];
        unsigned long long sender = recv(msg);
        
        if (sender == (unsigned long long)-1) {
            asm volatile("pause");
            continue;
        }
        
        unsigned long long cmd = msg[0];
        
        // Store sender in msg[0] for response
        unsigned long long my_msg[8];
        my_msg[0] = sender;
        for (int i = 1; i < 8; i++) {
            my_msg[i] = msg[i-1];
        }
        
        switch (cmd) {
            case 1: // LOAD
                cmd_load(my_msg);
                break;
            case 2: // LIST
                cmd_list(sender);
                break;
            case 3: // START
                cmd_start(my_msg);
                break;
            default:
                write_str("ROOTMD: Unknown command ");
                char buf[8];
                itoa(cmd, buf);
                write_str(buf);
                write_str("\n");
                break;
        }
    }
}
