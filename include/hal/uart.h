// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_UART_H
#define HAL_UART_H

void hal_uart_init(void);
void hal_uart_drain_rx(void);
void hal_uart_putc(char c);
int  hal_uart_try_getc(void);
void hal_uart_set_rx_callback(void (*cb)(char));

#endif
