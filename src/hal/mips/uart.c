#include "hal/uart.h"
#include "io.h"
#include <stdint.h>

/*
 * QEMU MIPS Malta: ISA I/O base = physical 0x18000000
 * 16550 UART at ISA port 0x3F8 → physical 0x180003F8
 * Accessed via KSEG1 (uncached) at 0xB80003F8.
 */
#define UART_BASE 0xB80003F8

void hal_uart_init(void) {
    /* Minimal init: assume firmware/QEMU defaults are usable */
}

void hal_uart_drain_rx(void) {
    while (mmio_read8(UART_BASE + 5) & 0x01)
        (void)mmio_read8(UART_BASE);
}

void hal_uart_poll_rx(void) {
}

void hal_uart_putc(char c) {
    while ((mmio_read8(UART_BASE + 5) & 0x20) == 0) { }
    mmio_write8(UART_BASE, (uint8_t)c);
}

int hal_uart_try_getc(void) {
    if (mmio_read8(UART_BASE + 5) & 0x01) {
        return (int)mmio_read8(UART_BASE);
    }
    return -1;
}

void hal_uart_set_rx_callback(void (*cb)(char)) {
    (void)cb;
}
