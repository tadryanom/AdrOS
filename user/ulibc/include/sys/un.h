// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_UN_H
#define ULIBC_SYS_UN_H

#include <sys/socket.h>

#define UNIX_PATH_MAX 108

struct sockaddr_un {
    sa_family_t sun_family;
    char        sun_path[UNIX_PATH_MAX];
};

#endif
