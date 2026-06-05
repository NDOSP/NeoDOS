[BITS 64]
section .text
    global jumpToUserMode

    ; rdi = RIP
    ; rsi = RSP
    jumpToUserMode:
        cli

        mov ax, 0x1B
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax

        push 0x1B
        push rsi
        push 0x202
        push 0x23
        push rdi

        iretq