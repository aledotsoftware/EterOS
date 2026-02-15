
/*
 * éterOS - RK3566 Boot Entry (AArch64)
 */

.section .text.boot
.global _start

_start:
    /* Check CPU ID (MPIDR_EL1) - Only core 0 boots initially */
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbz x0, master_core
    /* Secondary cores loop */
    b hang

master_core:
    /* Setup Stack pointer */
    ldr x0, =stack_top
    mov sp, x0

    /* Clear BSS */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    
clear_bss:
    cmp x0, x1
    b.ge call_kernel
    str xzr, [x0], #8
    b clear_bss

call_kernel:
    /* Jump to C kernel */
    bl kmain
    
hang:
    wfi
    b hang

.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16KB Stack */
stack_top:
