ENTRY(efi_main)
SECTIONS
{
    . = 0;
    .text : {
        *(.text)
        *(.text.*)
    }
    .data : {
        *(.data)
        *(.data.*)
        *(.rodata)
        *(.rodata.*)
    }
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    /DISCARD/ : {
        *(.comment)
        *(.note.GNU-stack)
        *(.eh_frame)
    }
}
