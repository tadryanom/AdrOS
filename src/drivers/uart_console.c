// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include <stdint.h>
#include "uart_console.h"

#include "hal/uart.h"

void uart_init(void) {
    hal_uart_init();
}

void uart_put_char(char c) {
    hal_uart_putc(c);
}

void uart_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        uart_put_char(str[i]);
    }
}
