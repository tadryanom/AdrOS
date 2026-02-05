/*
 * AdrOS - MIPS32 Bootstrap
 * Minimal entry point that sets up a stack and calls kernel_main.
 */

    .set noreorder
    .set noat

    .section .text
    .globl _start
    .ent _start

_start:
    la   $sp, stack_top

    /* kernel_main(unsigned long magic, unsigned long addr)
     * For now, pass 0, 0 (no multiboot on MIPS)
     */
    move $a0, $zero
    move $a1, $zero

    jal  kernel_main
    nop

1:
    j    1b
    nop

    .end _start

    .section .bss
    .align 16
stack_bottom:
    .space 16384
stack_top:
