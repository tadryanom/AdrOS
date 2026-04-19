// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
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
    if (umount(argv[1]) < 0) {
        fprintf(stderr, "umount: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    return 0;
}
