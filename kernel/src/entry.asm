[BITS 64]

extern kmain

section .text
    global _start
    _start:
        mov rsp, 0xFFFFFFFFFFFFFFFF
        mov rbp, rsp
        call kmain
        hlt