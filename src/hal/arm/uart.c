#include "hal/uart.h"
#include <stdint.h>

#define UART_BASE 0x09000000

void hal_uart_init(void) {
}

void hal_uart_drain_rx(void) {
}

void hal_uart_poll_rx(void) {
}

void hal_uart_putc(char c) {
    volatile uint32_t* uart = (volatile uint32_t*)UART_BASE;
    while (uart[6] & (1 << 5)) { }
    uart[0] = (uint32_t)c;
}

int hal_uart_try_getc(void) {
    volatile uint32_t* uart = (volatile uint32_t*)UART_BASE;
    if (!(uart[6] & (1 << 4))) {
        return (int)(uart[0] & 0xFF);
    }
    return -1;
}

void hal_uart_set_rx_callback(void (*cb)(char)) {
    (void)cb;
}
