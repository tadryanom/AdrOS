// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_GRP_H
#define ULIBC_GRP_H

#include <stddef.h>

struct group {
    char*  gr_name;
    char*  gr_passwd;
    int    gr_gid;
    char** gr_mem;
};

struct group* getgrnam(const char* name);
struct group* getgrgid(int gid);
void          setgrent(void);
void          endgrent(void);
struct group* getgrent(void);

#endif
