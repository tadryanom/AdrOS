// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

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
#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSETSW    0x5403
#define TCSETSF    0x5404
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410

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

typedef unsigned int speed_t;

#define B0      0
#define B9600   9600
#define B19200  19200
#define B38400  38400
#define B115200 115200

speed_t cfgetispeed(const struct termios* t);
speed_t cfgetospeed(const struct termios* t);
int     cfsetispeed(struct termios* t, speed_t speed);
int     cfsetospeed(struct termios* t, speed_t speed);
void    cfmakeraw(struct termios* t);
int     tcdrain(int fd);
int     tcflush(int fd, int queue_selector);
int     tcflow(int fd, int action);
int     tcsendbreak(int fd, int duration);
int     tcgetpgrp(int fd);
int     tcsetpgrp(int fd, int pgrp);

#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2
#define TCOOFF    0
#define TCOON     1
#define TCIOFF    2
#define TCION     3

#endif
