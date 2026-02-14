// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "hal/uart.h"
#include "io.h"

#define UART_BASE 0x10000000

void hal_uart_init(void) {
    mmio_write8(UART_BASE + 3, 0x03);
    mmio_write8(UART_BASE + 2, 0x01);
    mmio_write8(UART_BASE + 1, 0x01);
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
