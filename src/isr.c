/* 
 * High level interrupt service routines and interrupt request handlers.
 * Based on code from Bran's and JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#include <isr.h>
#include <system.h>
#include <stdio.h>

isr_t interrupt_handlers[IDT_ENTRIES];

void register_interrupt_handler (u8int n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

// This gets called from our ASM interrupt handler stub.
void isr_handler (registers_t regs)
{
    /* 
     * This line is important. When the processor extends the 8-bit interrupt number
     * to a 32bit value, it sign-extends, not zero extends. So if the most significant
     * bit (0x80) is set, regs.int_no will be very large (about 0xffffff80).
     */
    u8int int_no = regs.int_no & 0xFF;
    if (interrupt_handlers[int_no] != 0) {
        isr_t handler = interrupt_handlers[int_no];
        handler(&regs);
    } else {
        printf("unhandled interrupt: 0x%x\n", int_no);
        for(;;){}
    }
}

// This gets called from our ASM interrupt handler stub.
void irq_handler (registers_t regs)
{
    /*
     * Send an EOI (end of interrupt) signal to the PICs.
     * If this interrupt involved the slave.
     */
    if (regs.int_no >= 40) {
        outportb(0xA0, 0x20); // Send reset signal to slave.
    }
    outportb(0x20, 0x20); // Send reset signal to master. (As well as slave, if necessary).

    if (interrupt_handlers[regs.int_no] != 0) {
        isr_t handler = interrupt_handlers[regs.int_no];
        handler(&regs);
    }
}
