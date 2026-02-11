/*
 * AP Trampoline — 16-bit real-mode entry point for Application Processors.
 *
 * This code is copied to physical address 0x8000 before sending INIT-SIPI-SIPI.
 * Each AP starts executing here in 16-bit real mode.
 *
 * Data layout at fixed physical addresses (written by BSP before SIPI):
 *   0x8F00: GDT pointer (6 bytes: limit + phys base)
 *   0x8F08: CR3 value (page directory physical address, 4 bytes)
 *   0x8F0C: AP kernel stack top (virtual address, 4 bytes)
 *   0x8F10: C entry point (virtual address, 4 bytes)
 */

.set AP_TRAMPOLINE_BASE, 0x8000
.set AP_DATA_BASE,       0x8F00

.code16
.section .ap_trampoline, "ax", @progbits
.global ap_trampoline_start
.global ap_trampoline_end

ap_trampoline_start:
    cli
    cld

    /* DS = 0 so we can use absolute physical addresses */
    xor %ax, %ax
    mov %ax, %ds

    /* Load the GDT that the BSP prepared at 0x8F00 */
    lgdt (AP_DATA_BASE)

    /* Enable protected mode (PE bit in CR0) */
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0

    /* Far jump to 32-bit protected mode code.
     * 0x08 = kernel code segment selector.
     * Target address is patched by BSP. */
    .byte 0x66, 0xEA           /* 32-bit far jump opcode */
.global ap_pm_target
ap_pm_target:
    .long 0                     /* Patched: physical address of ap_pm_entry */
    .word 0x08                  /* Kernel CS selector */

.code32
.align 4
.global ap_pm_entry
ap_pm_entry:
    /* Now in 32-bit protected mode, paging OFF, DS base = 0 */
    mov $0x10, %ax              /* Kernel data segment selector */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Enable PAE in CR4 */
    mov %cr4, %eax
    or $0x20, %eax               /* CR4.PAE = bit 5 */
    mov %eax, %cr4

    /* Load CR3 (PDPT physical address) from data area */
    mov (AP_DATA_BASE + 8), %eax
    mov %eax, %cr3

    /* Enable paging */
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    /* Now paging is ON — we're still executing from identity-mapped low memory.
     * Load the virtual stack and jump to the virtual C entry point. */
    mov (AP_DATA_BASE + 12), %esp
    mov (AP_DATA_BASE + 16), %eax
    jmp *%eax

    /* Should never reach here */
1:  hlt
    jmp 1b

ap_trampoline_end:

.section .note.GNU-stack,"",@progbits
