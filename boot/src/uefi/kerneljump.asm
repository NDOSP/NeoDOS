[BITS 64]

section .text
    global kernelJump_start
    global kernelJump_end

    global kernelJump
    align 4096
    kernelJump_start:
    kernelJump:
        mov cr3, rsi
        cli
        jmp rdi
    kernelJump_end:

    times 4096 - ($ - $$) db 0xCC