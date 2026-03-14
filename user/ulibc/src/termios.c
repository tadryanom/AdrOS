// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "termios.h"
#include "syscall.h"
#include "errno.h"
#include "string.h"

speed_t cfgetispeed(const struct termios* t) {
    (void)t;
    return B115200;
}

speed_t cfgetospeed(const struct termios* t) {
    (void)t;
    return B115200;
}

int cfsetispeed(struct termios* t, speed_t speed) {
    (void)t; (void)speed;
    return 0;
}

int cfsetospeed(struct termios* t, speed_t speed) {
    (void)t; (void)speed;
    return 0;
}

void cfmakeraw(struct termios* t) {
    t->c_iflag &= ~(ICRNL | IGNCR | INLCR);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO | ICANON | ISIG);
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}

int tcdrain(int fd) {
    return __syscall_ret(_syscall2(SYS_IOCTL, fd, TCSETSW));
}

int tcflush(int fd, int queue_selector) {
    (void)queue_selector;
    return __syscall_ret(_syscall2(SYS_IOCTL, fd, TCSETSF));
}

int tcflow(int fd, int action) {
    (void)fd; (void)action;
    return 0;
}

int tcsendbreak(int fd, int duration) {
    (void)fd; (void)duration;
    return 0;
}

int tcgetpgrp(int fd) {
    int pgrp = 0;
    int ret = _syscall3(SYS_IOCTL, fd, TIOCGPGRP, (int)&pgrp);
    if (ret < 0) { errno = -ret; return -1; }
    return pgrp;
}

int tcsetpgrp(int fd, int pgrp) {
    return __syscall_ret(_syscall3(SYS_IOCTL, fd, TIOCSPGRP, (int)&pgrp));
}
