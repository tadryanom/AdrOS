// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_PWD_H
#define ULIBC_PWD_H

#include <stddef.h>

struct passwd {
    char*  pw_name;
    char*  pw_passwd;
    int    pw_uid;
    int    pw_gid;
    char*  pw_gecos;
    char*  pw_dir;
    char*  pw_shell;
};

struct passwd* getpwnam(const char* name);
struct passwd* getpwuid(int uid);
void           setpwent(void);
void           endpwent(void);
struct passwd* getpwent(void);

/* Password verification against /etc/shadow */
int check_password(const char* username, const char* password);

#endif
