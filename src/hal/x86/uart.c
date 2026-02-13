#include "hal/uart.h"
#include "io.h"

#include <stdint.h>

#define UART_BASE 0x3F8

void hal_uart_init(void) {
    outb(UART_BASE + 1, 0x00);
    outb(UART_BASE + 3, 0x80);
    outb(UART_BASE + 0, 0x03);
    outb(UART_BASE + 1, 0x00);
    outb(UART_BASE + 3, 0x03);
    outb(UART_BASE + 2, 0xC7);
    outb(UART_BASE + 4, 0x0B);
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
