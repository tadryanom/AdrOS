/*
 * AdrOS - RISC-V 64-bit Bootstrap
 * Target: QEMU 'virt' machine (starts at 0x80000000)
 */

.section .text
.global _start

_start:
    /* Disable interrupts (sie = Supervisor Interrupt Enable) */
    csrw sie, zero

    /* Set up stack pointer.
     * In linker.ld we will define a symbol for the stack top.
     */
    la sp, stack_top

    /* Build arch_boot_args (in .bss) and call arch_early_setup(args) */
    la a0, arch_boot_args
    call arch_early_setup

    /* Hang if return */
1:  wfi
    j 1b

.section .bss
.align 16
arch_boot_args:
    .skip 16
stack_bottom:
    .skip 16384 /* 16KB Stack */
stack_top:
