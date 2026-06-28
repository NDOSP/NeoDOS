[BITS 64]
section .rodata
    global trampoline_start
    global trampoline_end
    trampoline_start:
        incbin "build/trampoline.bin"
    trampoline_end:
