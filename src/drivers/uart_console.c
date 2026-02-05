#include <stdint.h>
#include "uart_console.h"

#include "hal/uart.h"

#include "spinlock.h"

static spinlock_t uart_lock = {0};

void uart_init(void) {
    hal_uart_init();
}

void uart_put_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&uart_lock);
    hal_uart_putc(c);
    spin_unlock_irqrestore(&uart_lock, flags);
}

void uart_print(const char* str) {
    uintptr_t flags = spin_lock_irqsave(&uart_lock);
    for (int i = 0; str[i] != '\0'; i++) {
        hal_uart_putc(str[i]);
    }
    spin_unlock_irqrestore(&uart_lock, flags);
}
