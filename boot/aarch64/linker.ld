EXTERN(_start)
ENTRY(_start)

MEMORY
{
    /* RK3566 DRAM usually starts at 0x0000_0000 or high? */
    /* Wait, System Memory map... 0x0000_0000 usually DRAM. */
    /* Most distros load kernel at 0x0020_0000 or similar? */
    /* Let's assume standard QEMU virt loading at 0x40080000 */
    /* For RK3566 specifically, U-Boot loads it where specified. */
    /* We'll link at 0x00200000 (2MB) assuming load there. */
    
    RAM (rwx) : ORIGIN = 0x00200000, LENGTH = 128M
}

SECTIONS
{
    . = 0x00200000;

    .text :
    {
        KEEP(*(.text.boot))
        *(.text)
    } > RAM

    .rodata :
    {
        *(.rodata)
    } > RAM

    .data :
    {
        *(.data)
    } > RAM

    .bss :
    {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    } > RAM
    
    /* Symbols for heap manager */
    _kernel_end = .;
}
