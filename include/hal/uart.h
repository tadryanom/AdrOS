// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_UART_H
#define HAL_UART_H

void hal_uart_init(void);
void hal_uart_putc(char c);
int  hal_uart_try_getc(void);

#endif
