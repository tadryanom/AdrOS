// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS pwd utility — print working directory */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    char buf[256];
    if (getcwd(buf, sizeof(buf)) >= 0)
        printf("%s\n", buf);
    else {
        fprintf(stderr, "pwd: error\n");
        return 1;
    }
    return 0;
}
