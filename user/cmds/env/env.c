// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS env utility — print environment or run command with modified env */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char** argv) {
    if (argc <= 1) {
        /* Print all environment variables */
        if (environ) {
            for (int i = 0; environ[i]; i++)
                printf("%s\n", environ[i]);
        }
        return 0;
    }
    /* env COMMAND ARGS... — run command with current environment */
    execve(argv[1], &argv[1], environ);
    fprintf(stderr, "env: %s: not found\n", argv[1]);
    return 127;
}
