#include "idt.h"
#include "io.h"
#include "uart_console.h"
#include <stddef.h>

#define IDT_ENTRIES 256

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr idtp;

// Array of function pointers for handlers
isr_handler_t interrupt_handlers[IDT_ENTRIES];

// Extern prototypes for Assembly stubs
extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0(); extern void irq1(); extern void irq2(); extern void irq3();
extern void irq4(); extern void irq5(); extern void irq6(); extern void irq7();
extern void irq8(); extern void irq9(); extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags; // Present(0x80) | DPL(00) | Type(0xE = 32bit Int Gate) -> 0x8E
}

/* Reconfigure the PIC to remap IRQs from 0-15 to 32-47 */
void pic_remap(void) {
    uint8_t a1, a2;

    a1 = inb(0x21); // Save masks
    a2 = inb(0xA1);

    outb(0x20, 0x11); // Start Init (ICW1)
    outb(0xA0, 0x11);

    outb(0x21, 0x20); // Master offset: 32 (0x20) (ICW2)
    outb(0xA1, 0x28); // Slave offset: 40 (0x28)

    outb(0x21, 0x04); // Tell Master about Slave (ICW3)
    outb(0xA1, 0x02); // Tell Slave its cascade identity

    outb(0x21, 0x01); // 8086 mode (ICW4)
    outb(0xA1, 0x01);

    outb(0x21, a1);   // Restore masks
    outb(0xA1, a2);
}

void idt_init(void) {
    uart_print("[IDT] Initializing Interrupts...\n");
    
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (uint32_t)&idt;

    // Clear memory
    for (int i=0; i<IDT_ENTRIES; i++) {
        // Default to 0, non-present
        idt[i].base_lo = 0;
        idt[i].sel = 0;
        idt[i].always0 = 0;
        idt[i].flags = 0;
        interrupt_handlers[i] = NULL;
    }

    // Remap PIC
    pic_remap();

    // Install Exception Handlers (0-31)
    // 0x08 is our Kernel Code Segment
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E); // Page Fault!
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    // Install IRQ Handlers (32-47)
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E); // Timer
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E); // Keyboard
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // Load IDT
    __asm__ volatile("lidt %0" : : "m"(idtp));
    
    // Enable interrupts!
    __asm__ volatile("sti");
    
    uart_print("[IDT] Interrupts Enabled!\n");
}

void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

// The Main Handler called by assembly
void isr_handler(struct registers* regs) {
    // Check if we have a custom handler
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        // If Exception (0-31), Panic
        if (regs->int_no < 32) {
            uart_print("\n[PANIC] Unhandled Exception: ");
            // TODO: Print int_no hex
            uart_print(" (Halting)\n");
            
            // Print Page Fault specifics
            if (regs->int_no == 14) {
                uart_print("Page Fault! Address: ");
                uint32_t cr2;
                __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
                // Print CR2
            }
            
            for(;;) __asm__("hlt");
        }
    }
    
    // Send EOI (End of Interrupt) to PICs for IRQs (32-47)
    if (regs->int_no >= 32) {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20); // Slave EOI
        }
        outb(0x20, 0x20); // Master EOI
    }
}
