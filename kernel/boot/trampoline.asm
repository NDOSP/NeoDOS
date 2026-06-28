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

    mov eax, [pml4_addr]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp 0x08:start64

[BITS 64]
align 16
start64:
    mov rsp, [ap_stack]
    mov rax, [ap_entry]
    call rax
    hlt

align 16
gdt_start:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd 0x5000 + (gdt_start - start16)

times 0x200 - ($ - $$) db 0x00

align 8
pml4_addr: dd 0
ap_stack:  dq 0
ap_entry:  dq 0
trampoline_end:
