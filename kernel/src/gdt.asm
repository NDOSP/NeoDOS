[BITS 64]

section .data
    align 16

    global gdt64
    gdt64:
        .null: dq 0 ; Null
        .code: dq 0x00209A0000000000   ; Kernel code (0x08): present, ring0, exec/read, 64-bit
        .data: dq 0x0000920000000000   ; Kernel data (0x10): present, ring0, read/write

        .tss_low: dq 0 ; TSS Low
        .tss_high: dq 0 ; TSS High

    gdt64_ptr:
        dw $ - gdt64 - 1
        dq gdt64

section .text
    global loadGdt
    loadGdt:
        lgdt [gdt64_ptr]

        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax

        mov ax, 0x18
        ltr ax

        push 0x08
        lea rax, [rel .reload_cs]
        push rax
        retfq

    .reload_cs:
        ret