#ifndef HAL_UART_H
#define HAL_UART_H

void hal_uart_init(void);
void hal_uart_drain_rx(void);
void hal_uart_putc(char c);
int  hal_uart_try_getc(void);
void hal_uart_set_rx_callback(void (*cb)(char));

#endif
