OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 0;

    .modinfo : {
        KEEP(*(.modinfo))
    }

    __modtable_start = .;
    .modtable : {
        KEEP(*(.modtable.table))
        KEEP(*(.modtable.function))
        KEEP(*(.modtable.*))
        KEEP(*(.modtable))
    }
    __modtable_end = .;

    .text : {
        *(.text.start)
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.note.gnu.property)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __bss_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
