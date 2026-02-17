// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* AdrOS df utility — display filesystem disk space usage */
#include <stdio.h>

int main(void) {
    printf("Filesystem     Size  Used  Avail  Use%%  Mounted on\n");
    printf("overlayfs         -     -      -     -  /\n");
    printf("devfs             -     -      -     -  /dev\n");
    printf("procfs            -     -      -     -  /proc\n");
    return 0;
}
