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
#include "errno.h"

long sysconf(int name) {
    switch (name) {
    case _SC_CLK_TCK:      return 100;
    case _SC_PAGE_SIZE:    return 4096;
    case _SC_OPEN_MAX:     return 64;
    case _SC_NGROUPS_MAX:  return 0;
    case _SC_CHILD_MAX:    return 128;
    case _SC_ARG_MAX:      return 131072;
    case _SC_HOST_NAME_MAX: return 64;
    case _SC_LOGIN_NAME_MAX: return 32;
    case _SC_LINE_MAX:     return 2048;
    default:
        errno = EINVAL;
        return -1;
    }
}

long pathconf(const char* path, int name) {
    (void)path;
    switch (name) {
    case _PC_PATH_MAX:     return 256;
    case _PC_NAME_MAX:     return 255;
    case _PC_PIPE_BUF:     return 4096;
    case _PC_LINK_MAX:     return 127;
    default:
        errno = EINVAL;
        return -1;
    }
}

long fpathconf(int fd, int name) {
    (void)fd;
    return pathconf("/", name);
}
