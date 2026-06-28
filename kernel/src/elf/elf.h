#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_MAGIC      0x464C457F
#define PT_NULL        0
#define PT_LOAD        1

#define USER_STACK_VADDR 0x7FFFF0000000ULL
#define USER_STACK_SIZE  (4 * 4096)

typedef struct {
    uint8_t  e_ident[16];
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
} __attribute__((packed)) Elf64Header;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64Phdr;

int elfLoad(void* data, uint64_t* entry, uint64_t* stackTop);

#endif
