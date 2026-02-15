/*
 * AdrOS - ARM64 (AArch64) Bootstrap
 * Target: QEMU 'virt' machine (-cpu cortex-a57)
 */

.section .text
.global _start

_start:
    /* Disable all interrupts */
    msr daifset, #0xf

    /* Check current exception level */
    mrs x0, CurrentEL
    lsr x0, x0, #2
    cmp x0, #2
    b.ne 1f

    /* EL2 -> EL1 transition */
    mov x0, #(1 << 31)         /* EL1 is AArch64 */
    msr hcr_el2, x0
    mov x0, #0x3c5             /* EL1h, all DAIF masked */
    msr spsr_el2, x0
    adr x0, 1f
    msr elr_el2, x0
    eret

1:
    /* Now in EL1 (or started in EL1) */

    /* Enable FP/SIMD — required for variadic function prologues
     * which save q0-q7 as part of the va_list register save area.
     * Without this, any variadic call (kprintf etc.) traps. */
    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)     /* FPEN = 0b11 (no trapping) */
    msr cpacr_el1, x0
    isb

    /* Set up stack pointer */
    ldr x0, =stack_top
    mov sp, x0

    /* Zero BSS section */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
3:  cmp x0, x1
    b.ge 4f
    str xzr, [x0], #8
    b 3b
4:
    /* Build arch_boot_args (in .bss) and call arch_early_setup(args) */
    ldr x0, =arch_boot_args
    bl arch_early_setup

    /* Hang */
2:  wfi
    b 2b

.section .bss
.align 16
arch_boot_args:
    .skip 16
stack_bottom:
    .skip 16384
stack_top:
