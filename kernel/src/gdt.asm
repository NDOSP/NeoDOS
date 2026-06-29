[BITS 64]

section .data
    align 16

    global gdt64
    gdt64:
        .null: dq 0
        .code: dq 0x00209A0000000000  ; Kernel code (0x08): ring0, exec/read, 64-bit
        .code2: dq 0x00209A0000000000 ; Kernel code2 (0x10): ring0, exec/read, 64-bit (SYSCALL CS)
        .data: dq 0x0000920000000000  ; Kernel data (0x18): ring0, read/write (SYSCALL SS)
        .user_data: dq 0x0000F20000000000 ; User data (0x20): ring3, read/write
        .user_code: dq 0x0020FA0000000000 ; User code (0x28): ring3, exec/read, 64-bit
        .pad: dq 0                      ; 0x30 Padding
        .user_code_sysret: dq 0x0020FA0000000000 ; User code (0x38): ring3, for Intel SYSRET CS

        .tss_array: times 64 dq 0 ; TSS descriptors (starting at 0x40)

    gdt64_ptr:
        dw $ - gdt64 - 1
        dq gdt64

section .text
    global loadGdt
    loadGdt:
        lgdt [gdt64_ptr]

        mov rax, rdi
        shl rax, 4
        add rax, 0x40
        ltr ax

        mov ax, 0x18
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