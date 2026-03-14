// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* AdrOS id utility — display user and group IDs */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("uid=%d gid=%d euid=%d egid=%d\n",
           getuid(), getgid(), geteuid(), getegid());
    return 0;
}
