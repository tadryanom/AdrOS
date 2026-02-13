#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>

#define NCCS  11

#define VINTR  0   /* Ctrl-C  → SIGINT  */
#define VQUIT  1   /* Ctrl-\  → SIGQUIT */
#define VERASE 2   /* Backspace / DEL    */
#define VKILL  3   /* Ctrl-U  (kill line)*/
#define VEOF   4   /* Ctrl-D  (EOF)      */
#define VSUSP  7   /* Ctrl-Z  → SIGTSTP  */
#define VMIN   8
#define VTIME  9

struct termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_cc[NCCS];
};

struct winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

/* c_lflag bits */
enum {
    TTY_ICANON = 0x0002,
    TTY_ECHO   = 0x0008,
    TTY_ISIG   = 0x0001,
};

/* c_iflag bits */
enum {
    TTY_ICRNL  = 0x0100,  /* map CR to NL on input        */
    TTY_IGNCR  = 0x0080,  /* ignore CR on input            */
    TTY_INLCR  = 0x0040,  /* map NL to CR on input         */
};

/* c_oflag bits (POSIX) */
enum {
    TTY_OPOST = 0x0001,
    TTY_ONLCR = 0x0004,
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
