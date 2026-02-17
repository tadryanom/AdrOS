#include "hal/uart.h"
#include "io.h"
#include "arch/x86/idt.h"

#include <stdint.h>

#define UART_BASE 0x3F8

static int uart_present = 0;
static void (*uart_rx_cb)(char) = 0;

static void uart_irq_handler(struct registers* regs) {
    (void)regs;
    while (inb(UART_BASE + 5) & 0x01) {
        char c = (char)inb(UART_BASE);
        if (uart_rx_cb) uart_rx_cb(c);
    }
}

void hal_uart_init(void) {
    /* Detect UART hardware via scratch register (offset 7).
     * Write a test value, read it back.  If no 16550 is present the
     * floating ISA bus returns 0xFF for all reads, so the test fails. */
    outb(UART_BASE + 7, 0xA5);
    if (inb(UART_BASE + 7) != 0xA5) {
        uart_present = 0;
        return;  /* No UART — skip all configuration */
    }
    outb(UART_BASE + 7, 0x5A);
    if (inb(UART_BASE + 7) != 0x5A) {
        uart_present = 0;
        return;
    }
    uart_present = 1;

    outb(UART_BASE + 1, 0x00);    /* Disable all interrupts */
    outb(UART_BASE + 3, 0x80);    /* Enable DLAB */
    outb(UART_BASE + 0, 0x03);    /* Baud 38400 */
    outb(UART_BASE + 1, 0x00);
    outb(UART_BASE + 3, 0x03);    /* 8N1 */
    outb(UART_BASE + 2, 0x07);    /* Enable FIFO, clear both, 1-byte trigger */
    outb(UART_BASE + 4, 0x0B);    /* DTR + RTS + OUT2 */

    /* Register IRQ 4 handler (IDT vector 36 = 32 + 4) */
    register_interrupt_handler(36, uart_irq_handler);

    /* Enable receive data available interrupt (IER bit 0) */
    outb(UART_BASE + 1, 0x01);
}

int hal_uart_is_present(void) {
    return uart_present;
}

void hal_uart_drain_rx(void) {
    if (!uart_present) return;
    /* Full UART interrupt reinitialisation for IOAPIC hand-off.
     *
     * hal_uart_init() runs under the legacy PIC and enables IER bit 0
     * (RX interrupt).  By the time the IOAPIC routes IRQ 4 as
     * edge-triggered, the UART IRQ line may already be asserted —
     * the IOAPIC will never see a rising edge and serial input is
     * permanently dead.
     *
     * Fix: temporarily disable ALL UART interrupts so the IRQ line
     * goes LOW, drain every pending condition, then re-enable IER.
     * The next character will produce a clean LOW→HIGH edge. */

    /* 1. Disable all UART interrupts — IRQ line goes LOW */
    outb(UART_BASE + 1, 0x00);

    /* 2. Drain the RX FIFO */
    while (inb(UART_BASE + 5) & 0x01)
        (void)inb(UART_BASE);

    /* 3. Read IIR until "no interrupt pending" (bit 0 set) */
    for (int i = 0; i < 16; i++) {
        uint8_t iir = inb(UART_BASE + 2);
        if (iir & 0x01) break;
    }

    /* 4. Clear modem-status delta bits */
    (void)inb(UART_BASE + 6);

    /* 5. Clear line-status error bits */
    (void)inb(UART_BASE + 5);

    /* 6. Re-enable RX interrupt — next character will assert a clean edge */
    outb(UART_BASE + 1, 0x01);
}

void hal_uart_poll_rx(void) {
    if (!uart_present) return;
    /* Timer-driven fallback: drain any pending characters from the
     * UART FIFO via polling.  Called from the timer tick handler so
     * serial input works even if the IOAPIC edge-triggered IRQ for
     * COM1 is never delivered (observed in QEMU i440FX). */
    while (inb(UART_BASE + 5) & 0x01) {
        char c = (char)inb(UART_BASE);
        if (uart_rx_cb) uart_rx_cb(c);
    }
}

void hal_uart_set_rx_callback(void (*cb)(char)) {
    uart_rx_cb = cb;
}

void hal_uart_putc(char c) {
    if (!uart_present) return;
    int timeout = 100000;
    while ((inb(UART_BASE + 5) & 0x20) == 0 && --timeout > 0) { }
    outb(UART_BASE, (uint8_t)c);
}

int hal_uart_try_getc(void) {
    if (!uart_present) return -1;
    if (inb(UART_BASE + 5) & 0x01) {
        return (int)inb(UART_BASE);
    }
    return -1;
}
