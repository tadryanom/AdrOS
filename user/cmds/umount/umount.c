// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* AdrOS umount utility — unmount filesystems */
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <errno.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "umount: missing operand\n");
        return 1;
    }

    int flags = 0;
    const char* target = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            flags |= MNT_FORCE;
        } else if (strcmp(argv[i], "-l") == 0) {
            flags |= MNT_DETACH;
        } else {
            target = argv[i];
        }
    }

    if (!target) {
        fprintf(stderr, "umount: missing operand\n");
        return 1;
    }

    if (umount2(target, flags) < 0) {
        fprintf(stderr, "umount: %s: %s\n", target, strerror(errno));
        return 1;
    }
    return 0;
}
