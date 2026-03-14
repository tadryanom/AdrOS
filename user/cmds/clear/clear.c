// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS clear utility — clear the terminal screen */
#include <unistd.h>

int main(void) {
    /* ANSI escape: clear screen + move cursor to top-left */
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
    return 0;
}
