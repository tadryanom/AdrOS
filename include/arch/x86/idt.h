#ifndef ARCH_X86_IDT_H
#define ARCH_X86_IDT_H

#include <stdint.h>

/* IDT Entry (Gate Descriptor) */
struct idt_entry {
    uint16_t base_lo;   // Lower 16 bits of handler address
    uint16_t sel;       // Kernel segment selector
    uint8_t  always0;   // This must always be zero
    uint8_t  flags;     // Type and attributes
    uint16_t base_hi;   // Upper 16 bits of handler address
} __attribute__((packed));

/* IDT Pointer (Loaded into IDTR) */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Registers saved by our assembly ISR stub */
struct registers {
    uint32_t gs;                                     // Per-CPU GS selector (pushed second)
    uint32_t ds;                                     // Data segment selector (pushed first)
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t int_no, err_code;                       // Interrupt number and error code
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the processor automatically
};

// Initialize IDT and PIC
void idt_init(void);

// Load IDT on an AP (same IDT as BSP, just needs lidt)
void idt_load_ap(void);

// Register a custom handler for a specific interrupt
typedef void (*isr_handler_t)(struct registers*);
void register_interrupt_handler(uint8_t n, isr_handler_t handler);
void unregister_interrupt_handler(uint8_t n, isr_handler_t handler);

#endif
