#ifndef ULIBC_TERMIOS_H
#define ULIBC_TERMIOS_H

#include <stdint.h>

#define NCCS 11

/* c_lflag bits */
#define ISIG    0x0001
#define ICANON  0x0002
#define ECHO    0x0008

/* c_iflag bits */
#define ICRNL   0x0100
#define IGNCR   0x0080
#define INLCR   0x0040

/* c_oflag bits */
#define OPOST   0x0001
#define ONLCR   0x0004

/* c_cc indices */
#define VINTR   0
#define VQUIT   1
#define VERASE  2
#define VKILL   3
#define VEOF    4
#define VSUSP   7
#define VMIN    8
#define VTIME   9

/* ioctl commands */
#define TCGETS   0x5401
#define TCSETS   0x5402
#define TCSETSW  0x5403
#define TCSETSF  0x5404

struct termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_cc[NCCS];
};

int tcgetattr(int fd, struct termios* t);
int tcsetattr(int fd, int actions, const struct termios* t);

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#endif
