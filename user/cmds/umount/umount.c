// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS umount utility — stub (no SYS_UMOUNT syscall yet) */
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "umount: missing operand\n");
        return 1;
    }
    fprintf(stderr, "umount: %s: operation not supported\n", argv[1]);
    return 1;
}
