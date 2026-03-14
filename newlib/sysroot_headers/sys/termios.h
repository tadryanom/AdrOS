// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H

#include <sys/types.h>

#define NCCS 32

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_cc[NCCS];
    speed_t  c_ispeed;
    speed_t  c_ospeed;
};

/* c_lflag bits */
#define ECHO    0x0008
#define ECHOE   0x0010
#define ECHOK   0x0020
#define ECHONL  0x0040
#define ICANON  0x0002
#define ISIG    0x0001
#define NOFLSH  0x0080
#define TOSTOP  0x0100
#define IEXTEN  0x8000
#define ECHOCTL 0x0200
#define ECHOKE  0x0400

/* c_iflag bits */
#define IGNBRK  0x0001
#define BRKINT  0x0002
#define IGNPAR  0x0004
#define PARMRK  0x0008
#define INPCK   0x0010
#define ISTRIP  0x0020
#define INLCR   0x0040
#define IGNCR   0x0080
#define ICRNL   0x0100
#define IXON    0x0400
#define IXOFF   0x1000
#define IXANY   0x0800
#define IMAXBEL 0x2000

/* c_oflag bits */
#define OPOST   0x0001
#define ONLCR   0x0004

/* c_cflag bits */
#define CSIZE   0x0030
#define CS5     0x0000
#define CS6     0x0010
#define CS7     0x0020
#define CS8     0x0030
#define CSTOPB  0x0040
#define CREAD   0x0080
#define PARENB  0x0100
#define PARODD  0x0200
#define HUPCL   0x0400
#define CLOCAL  0x0800

/* c_cc indices */
#define VEOF    0
#define VEOL    1
#define VERASE  2
#define VKILL   3
#define VINTR   4
#define VQUIT   5
#define VSUSP   6
#define VSTART  7
#define VSTOP   8
#define VMIN    9
#define VTIME   10

/* tcsetattr actions */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TCOON 1
#define TCOOFF 0
#define TCION 2
#define TCIOFF 3
#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

/* Baud rates */
#define B0       0
#define B50      1
#define B75      2
#define B110     3
#define B134     4
#define B150     5
#define B200     6
#define B300     7
#define B600     8
#define B1200    9
#define B1800    10
#define B2400    11
#define B4800    12
#define B9600    13
#define B19200   14
#define B38400   15
#define B57600   16
#define B115200  17

int tcgetattr(int, struct termios *);
int tcsetattr(int, int, const struct termios *);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
speed_t cfgetispeed(const struct termios *);
speed_t cfgetospeed(const struct termios *);
int cfsetispeed(struct termios *, speed_t);
int cfsetospeed(struct termios *, speed_t);
int tcdrain(int);
int tcflow(int, int);
int tcflush(int, int);
int tcsendbreak(int, int);

#endif /* _SYS_TERMIOS_H */
