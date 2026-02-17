// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS uname utility — print system information */
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    const char* sysname = "AdrOS";
    const char* nodename = "adros";
    const char* release = "0.1.0";
    const char* version = "AdrOS x86 SMP";
    const char* machine = "i686";

    if (argc <= 1) { printf("%s\n", sysname); return 0; }

    int all = 0, s = 0, n = 0, r = 0, v = 0, m = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) all = 1;
        else if (strcmp(argv[i], "-s") == 0) s = 1;
        else if (strcmp(argv[i], "-n") == 0) n = 1;
        else if (strcmp(argv[i], "-r") == 0) r = 1;
        else if (strcmp(argv[i], "-v") == 0) v = 1;
        else if (strcmp(argv[i], "-m") == 0) m = 1;
    }
    if (all) { s = n = r = v = m = 1; }
    if (!s && !n && !r && !v && !m) s = 1;

    int first = 1;
    if (s) { printf("%s%s", first ? "" : " ", sysname); first = 0; }
    if (n) { printf("%s%s", first ? "" : " ", nodename); first = 0; }
    if (r) { printf("%s%s", first ? "" : " ", release); first = 0; }
    if (v) { printf("%s%s", first ? "" : " ", version); first = 0; }
    if (m) { printf("%s%s", first ? "" : " ", machine); first = 0; }
    printf("\n");
    return 0;
}
