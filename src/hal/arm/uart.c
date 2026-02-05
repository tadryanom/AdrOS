#include "hal/uart.h"
#include <stdint.h>

#define UART_BASE 0x09000000

void hal_uart_init(void) {
}

void hal_uart_putc(char c) {
    volatile uint32_t* uart = (volatile uint32_t*)UART_BASE;
    while (uart[6] & (1 << 5)) { }
    uart[0] = (uint32_t)c;
}
