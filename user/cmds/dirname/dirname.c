// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS dirname utility — strip last component from path */
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: dirname PATH\n");
        return 1;
    }
    char* p = argv[1];
    int len = (int)strlen(p);
    /* Remove trailing slashes */
    while (len > 1 && p[len - 1] == '/') len--;
    /* Find last slash */
    while (len > 0 && p[len - 1] != '/') len--;
    /* Remove trailing slashes from result */
    while (len > 1 && p[len - 1] == '/') len--;
    if (len == 0) { printf(".\n"); return 0; }
    p[len] = '\0';
    printf("%s\n", p);
    return 0;
}
