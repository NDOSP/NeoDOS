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
        *(.text*)
    } :text

    .data : ALIGN(4K) {
        *(.data*)
    } :data

    .bss (NOLOAD) : ALIGN(4K) {
        *(.bss*)
        *(COMMON)
    } :data

    .rodata : ALIGN(4K) {
        *(.rodata*)
    } :text

    .bootinfo : ALIGN(4K) {
        KEEP(*(.bootinfo*))
    } :bootinfo
}
