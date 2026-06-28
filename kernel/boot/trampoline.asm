[BITS 16]
[org 0x5000]

start16:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    lgdt [gdt_ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:start32

[BITS 32]
align 16
start32:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov eax, [0x5200]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp 0x18:start64

[BITS 64]
align 16
start64:
    mov rsp, [0x5204]
    mov rax, [0x520C]
    call rax
    hlt

align 16
gdt_start:
    dq 0                      ; Null descriptor
    dq 0x00CF9A000000FFFF     ; 32-bit code @ 0x08
    dq 0x00CF92000000FFFF     ; 32-bit data @ 0x10
    dq 0x00AF9A000000FFFF     ; 64-bit code @ 0x18
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd 0x5000 + (gdt_start - start16)

times 0x200 - ($ - $$) db 0x00
