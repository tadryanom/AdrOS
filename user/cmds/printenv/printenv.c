// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS printenv utility — print environment variables */
#include <stdio.h>
#include <string.h>

extern char** environ;

int main(int argc, char** argv) {
    if (!environ) return 1;
    if (argc <= 1) {
        for (int i = 0; environ[i]; i++)
            printf("%s\n", environ[i]);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        int found = 0;
        int nlen = (int)strlen(argv[i]);
        for (int j = 0; environ[j]; j++) {
            if (strncmp(environ[j], argv[i], (size_t)nlen) == 0 && environ[j][nlen] == '=') {
                printf("%s\n", environ[j] + nlen + 1);
                found = 1;
                break;
            }
        }
        if (!found) return 1;
    }
    return 0;
}
