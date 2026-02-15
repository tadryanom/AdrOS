/*
 * AdrOS - MIPS32r2 Bootstrap
 * Target: QEMU Malta board (little-endian, RAM at KSEG0 0x80000000)
 */

    .set noreorder
    .set mips32r2

    .section .text
    .globl _start
    .ent _start

_start:
    /* Disable interrupts: clear IE bit in CP0 Status */
    di
    ehb

    /* Set up stack pointer */
    la   $sp, stack_top

    /* Zero BSS section */
    la   $t0, __bss_start
    la   $t1, __bss_end
1:  bge  $t0, $t1, 2f
    nop
    sw   $zero, 0($t0)
    addiu $t0, $t0, 4
    j    1b
    nop

2:
    /* Call arch_early_setup(args) */
    la   $a0, arch_boot_args
    jal  arch_early_setup
    nop

    /* Hang if return */
3:  wait
    j    3b
    nop

    .end _start

    .section .bss
    .align 16
arch_boot_args:
    .space 16
stack_bottom:
    .space 16384
stack_top:
