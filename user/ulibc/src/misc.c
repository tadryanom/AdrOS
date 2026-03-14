// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "unistd.h"
#include "string.h"
#include "syscall.h"
#include "errno.h"

int gethostname(char* name, size_t len) {
    if (!name || len == 0) { errno = EINVAL; return -1; }
    const char* hostname = "adros";
    size_t hlen = strlen(hostname);
    if (hlen + 1 > len) { errno = ENAMETOOLONG; return -1; }
    memcpy(name, hostname, hlen + 1);
    return 0;
}

static char ttyname_buf[32];

char* ttyname(int fd) {
    if (!isatty(fd)) return (char*)0;
    strcpy(ttyname_buf, "/dev/tty");
    return ttyname_buf;
}

int pipe2(int fds[2], int flags) {
    int r = _syscall2(SYS_PIPE2, (int)fds, flags);
    return __syscall_ret(r);
}
