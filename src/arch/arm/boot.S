/*
 * AdrOS - ARM64 (AArch64) Bootstrap
 * Target: QEMU 'virt' machine
 */

.section .text
.global _start

_start:
    /* 
     * CPU usually starts in EL2 or EL3 on QEMU.
     * Ideally we should switch to EL1, but for a "hello world"
     * we will just set up the stack and run.
     */

    /* Set up stack pointer (x30 is link register, don't clobber it yet) */
    ldr x0, =stack_top
    mov sp, x0

    /* Jump to C kernel */
    bl kernel_main

    /* Hang */
1:  wfi
    b 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
