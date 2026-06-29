OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(4K) {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } :text

    . = ALIGN(4K);
    .data : ALIGN(4K) {
        *(.data)
        *(.data.*)
    } :data

    __bss_start = .;
    .bss : ALIGN(4K) {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data
    __bss_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
