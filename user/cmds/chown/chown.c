// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS chown utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chown <owner[:group]> <file>...\n");
        return 1;
    }

    /* Parse owner:group */
    int owner = -1, group = -1;
    char* colon = strchr(argv[1], ':');
    if (colon) {
        *colon = '\0';
        if (argv[1][0]) owner = atoi(argv[1]);
        if (colon[1]) group = atoi(colon + 1);
    } else {
        owner = atoi(argv[1]);
    }

    int rc = 0;
    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], owner, group) < 0) {
            fprintf(stderr, "chown: cannot change owner of '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
