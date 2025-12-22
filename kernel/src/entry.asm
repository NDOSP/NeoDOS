[BITS 64]

extern kmain
extern load_gdt

section .text
    global _start
    _start:
        mov rsp, 0xFFFFFFFFFFFFFFF0
        mov rbp, rsp
        call load_gdt
        call kmain
        hlt