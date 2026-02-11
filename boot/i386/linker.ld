/*
 * éterOS - Linker Script para i386 (32-bit Protected Mode)
 *
 * Layout de memoria idéntico al x86_64, pero con formato ELF32.
 *
 *   0x07C00 - 0x07DFF : Stage 1 del Bootloader (MBR, 512 bytes)
 *   0x07E00 - 0x09DFF : Stage 2 del Bootloader (~4 KB)
 *   0x10000 - ...      : Kernel de éterOS (32-bit)
 *   0x90000            : Tope del stack
 */

ENTRY(kmain)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    . = 0x10000;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text.boot)
        *(.text)
        *(.text.*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
