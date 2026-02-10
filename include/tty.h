#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>

struct termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
};

struct winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

enum {
    TTY_ICANON = 0x0002,
    TTY_ECHO   = 0x0008,
    TTY_ISIG   = 0x0001,
};

void tty_init(void);

int tty_read(void* user_buf, uint32_t len);
int tty_write(const void* user_buf, uint32_t len);

int tty_read_kbuf(void* kbuf, uint32_t len);
int tty_write_kbuf(const void* kbuf, uint32_t len);

int tty_can_read(void);
int tty_can_write(void);

int tty_ioctl(uint32_t cmd, void* user_arg);

void tty_input_char(char c);

#endif
