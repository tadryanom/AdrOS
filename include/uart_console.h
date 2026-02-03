#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

void uart_init(void);
void uart_put_char(char c);
void uart_print(const char* str);

#endif
