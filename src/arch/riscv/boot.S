/*
 * AdrOS - RISC-V 64-bit Bootstrap
 * Target: QEMU 'virt' machine (-bios none, starts at 0x80000000)
 */

.section .text
.global _start

_start:
    /* Disable interrupts */
    csrw mie, zero

    /* Set up stack pointer */
    la sp, stack_top

    /* Zero BSS section */
    la t0, __bss_start
    la t1, __bss_end
1:  bge t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b
2:
    /* Call arch_early_setup(args) */
    la a0, arch_boot_args
    call arch_early_setup

    /* Hang if return */
3:  wfi
    j 3b

.section .bss
.align 16
arch_boot_args:
    .skip 16
stack_bottom:
    .skip 16384 /* 16KB Stack */
stack_top:
