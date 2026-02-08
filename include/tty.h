#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>

void tty_init(void);

int tty_read(void* user_buf, uint32_t len);
int tty_write(const void* user_buf, uint32_t len);

int tty_read_kbuf(void* kbuf, uint32_t len);
int tty_write_kbuf(const void* kbuf, uint32_t len);

int tty_can_read(void);
int tty_can_write(void);

void tty_input_char(char c);

#endif
