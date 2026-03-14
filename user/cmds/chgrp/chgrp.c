// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS chgrp utility */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chgrp <group> <file>...\n");
        return 1;
    }

    int group = atoi(argv[1]);
    int rc = 0;

    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], -1, group) < 0) {
            fprintf(stderr, "chgrp: cannot change group of '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
