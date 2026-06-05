[BITS 64]

section .data
    align 16

    global gdt64
    gdt64:
        .null: dq 0 ; Null
        .code: dq 0x00209A0000000000  ; Kernel code (0x08): present, ring0, exec/read, 64-bit
        .data: dq 0x0000920000000000  ; Kernel data (0x10): present, ring0, read/write
        .user_data: dq 0x0000F20000000000 ; User data (0x18): present, ring3, read/write
        .user_code: dq 0x0020FA0000000000 ; User code (0x20): present, ring3, exec/read, 64-bit

        .tss_array: times 64 dq 0 ; TSS descriptors for up to 32 CPUs (starting at 0x28)

    gdt64_ptr:
        dw $ - gdt64 - 1
        dq gdt64

section .text
    global loadGdt
    loadGdt:
        lgdt [gdt64_ptr]

        mov rax, rdi
        shl rax, 4
        add rax, 0x28
        ltr ax

        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax

        push 0x08
        lea rax, [rel .reload_cs]
        push rax
        retfq

    .reload_cs:
        ret