[BITS 16]
[ORG 0x7C00]

jmp _start

print:
        push ax
        push bx
        mov ah, 0x0E
        mov bl, 0x0F
    .loop:
        lodsb
        cmp al, 0
        je .done
        int 0x10 
        jmp .loop
    .done:
        pop bx
        pop ax
        ret

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti   

    mov ax, 0x0003
    int 0x10

    mov si, error
    call print

    .hlt:
        hlt
        jmp .hlt

error:  db 0x0D, 0x0A
        db "================================", 0x0D, 0x0A
        db "NeoDOS Boot Error", 0x0D, 0x0A
        db "================================", 0x0D, 0x0A, 0x0A
        db "Error Name: UEFI_REQUIRED", 0x0D, 0x0A, 0x0A
        db "This system requires UEFI firmware.", 0x0D, 0x0A
        db "Your computer is booting in legacy BIOS mode.", 0x0D, 0x0A, 0x0A
        db "Please enable UEFI in your BIOS/UEFI settings.", 0x0D, 0x0A, 0x0A
        db "System halted.", 0x0D, 0x0A, 0

times 446 - ($ - $$) db 00