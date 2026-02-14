#include "hal/uart.h"
#include "io.h"
#include "arch/x86/idt.h"

#include <stdint.h>

#define UART_BASE 0x3F8

static void (*uart_rx_cb)(char) = 0;

static void uart_irq_handler(struct registers* regs) {
    (void)regs;
    while (inb(UART_BASE + 5) & 0x01) {
        char c = (char)inb(UART_BASE);
        if (uart_rx_cb) uart_rx_cb(c);
    }
}

void hal_uart_init(void) {
    outb(UART_BASE + 1, 0x00);    /* Disable all interrupts */
    outb(UART_BASE + 3, 0x80);    /* Enable DLAB */
    outb(UART_BASE + 0, 0x03);    /* Baud 38400 */
    outb(UART_BASE + 1, 0x00);
    outb(UART_BASE + 3, 0x03);    /* 8N1 */
    outb(UART_BASE + 2, 0xC7);    /* Enable FIFO */
    outb(UART_BASE + 4, 0x0B);    /* DTR + RTS + OUT2 */

    /* Register IRQ 4 handler (IDT vector 36 = 32 + 4) */
    register_interrupt_handler(36, uart_irq_handler);

    /* Enable receive data available interrupt (IER bit 0) */
    outb(UART_BASE + 1, 0x01);
}

void hal_uart_drain_rx(void) {
    /* Drain any pending characters from the UART FIFO.
     * This de-asserts the IRQ line so that the next character
     * produces a clean rising edge for the IOAPIC (edge-triggered). */
    (void)inb(UART_BASE + 2);          /* Read IIR to ack any pending */
    while (inb(UART_BASE + 5) & 0x01)  /* Drain RX FIFO */
        (void)inb(UART_BASE);
    (void)inb(UART_BASE + 6);          /* Read MSR to clear delta bits */
}

void hal_uart_set_rx_callback(void (*cb)(char)) {
    uart_rx_cb = cb;
}

void hal_uart_putc(char c) {
    int timeout = 100000;
    while ((inb(UART_BASE + 5) & 0x20) == 0 && --timeout > 0) { }
    outb(UART_BASE, (uint8_t)c);
}

int hal_uart_try_getc(void) {
    if (inb(UART_BASE + 5) & 0x01) {
        return (int)inb(UART_BASE);
    }
    return -1;
}
