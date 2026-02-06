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

    /* Build arch_boot_args (in .bss) and call arch_start(args) */
    la   $a0, arch_boot_args
    sw   $zero, 0($a0)
    sw   $zero, 4($a0)
    sw   $zero, 8($a0)
    sw   $zero, 12($a0)

    jal  arch_start
    nop

1:
    j    1b
    nop

    .end _start

    .section .bss
    .align 16
arch_boot_args:
    .space 16
stack_bottom:
    .space 16384
stack_top:
