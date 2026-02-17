// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* AdrOS rmdir utility — remove empty directories */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "rmdir: missing operand\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            fprintf(stderr, "rmdir: failed to remove '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
