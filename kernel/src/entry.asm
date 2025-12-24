[BITS 64]

extern kmain

section .text
    align 256
    global _start
    _start:
        mov rsp, 0xFFFFFFFFFFFFFFF0
        mov rbp, rsp
        call kmain
        hlt