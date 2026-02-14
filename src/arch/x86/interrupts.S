/*
 * AdrOS - x86 Interrupt Stubs
 */

.section .text
.extern isr_handler

/* Common handler stub */
isr_common_stub:
    /* Save all registers */
    pusha

    /* Save segment selectors (DS and GS saved separately) */
    mov %ds, %ax
    push %eax
    push %gs

    /* Load Kernel Data Segment for DS/ES/FS.
     * GS is NOT touched — it holds the per-CPU selector set by
     * percpu_setup_gs() and must survive ISR entry/exit. */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs

    /* Call C handler */
    push %esp   /* Pass pointer to stack structure */
    call isr_handler
    add $4, %esp

    /* Restore GS (per-CPU) then DS/ES/FS */
    pop %gs
    pop %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs

    /* Restore registers */
    popa

    /* Clean up error code and ISR number */
    add $8, %esp

    /* Return from interrupt */
    iret

/* Macro for exceptions without error code */
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli
    push $0       /* Dummy error code */
    push $\num    /* ISR Number */
    jmp isr_common_stub
.endm

/* Macro for exceptions WITH error code */
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli
    /* Error code is already pushed by CPU */
    push $\num    /* ISR Number */
    jmp isr_common_stub
.endm

/* Macro for IRQs (Hardware Interrupts) */
.macro IRQ num, map_to
.global irq\num
irq\num:
    cli
    push $0
    push $\map_to
    jmp isr_common_stub
.endm

/* 
 * Define the first 32 ISRs (Exceptions)
 * Some push error codes, some don't.
 */
ISR_NOERRCODE 0  /* Division by zero */
ISR_NOERRCODE 1  /* Debug */
ISR_NOERRCODE 2  /* NMI */
ISR_NOERRCODE 3  /* Breakpoint */
ISR_NOERRCODE 4  /* Overflow */
ISR_NOERRCODE 5  /* Bound Range Exceeded */
ISR_NOERRCODE 6  /* Invalid Opcode */
ISR_NOERRCODE 7  /* Device Not Available */
ISR_ERRCODE   8  /* Double Fault */
ISR_NOERRCODE 9  /* Coprocessor Segment Overrun */
ISR_ERRCODE   10 /* Invalid TSS */
ISR_ERRCODE   11 /* Segment Not Present */
ISR_ERRCODE   12 /* Stack-Segment Fault */
ISR_ERRCODE   13 /* General Protection Fault */
ISR_ERRCODE   14 /* Page Fault */
ISR_NOERRCODE 15 /* Reserved */
ISR_NOERRCODE 16 /* x87 Floating-Point Exception */
ISR_ERRCODE   17 /* Alignment Check */
ISR_NOERRCODE 18 /* Machine Check */
ISR_NOERRCODE 19 /* SIMD Floating-Point Exception */
ISR_NOERRCODE 20 /* Virtualization Exception */
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30 /* Security Exception */
ISR_NOERRCODE 31

/* 
 * IRQs (Mapped to 32-47)
 * IRQ 0 (Timer) -> Int 32
 * IRQ 1 (Keyboard) -> Int 33
 */
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

/* Syscall vector (int 0x80 -> 128) */
ISR_NOERRCODE 128

/* LAPIC spurious interrupt vector (255) */
ISR_NOERRCODE 255

 .section .note.GNU-stack,"",@progbits
