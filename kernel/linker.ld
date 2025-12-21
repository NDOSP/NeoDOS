OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);    /* RX */
    data PT_LOAD FLAGS(6);    /* RW */
    bootinfo PT_LOAD FLAGS(4); /* R */
}

SECTIONS {
    . = 0xFFFFFFFF80000000;

    .text : ALIGN(4K) {
        KEEP(*(.text*))
        . = ALIGN(16);
    } :text

    .data : ALIGN(4K) {
        KEEP(*(.data*))
        . = ALIGN(16);
        KEEP(*(.rodata*))
        . = ALIGN(16);
        KEEP(*(.bss*))
        *(COMMON)
        . = ALIGN(16);
    } :data

    .bootinfo : ALIGN(4K) {
        KEEP(*(.bootinfo*))
        . = ALIGN(16);
    } :bootinfo
}
