#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>

void tty_init(void);

int tty_read(void* user_buf, uint32_t len);
int tty_write(const void* user_buf, uint32_t len);

void tty_input_char(char c);

#endif
