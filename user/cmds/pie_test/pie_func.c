// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* Shared library function for PLT/GOT lazy binding test.
 * Compiled as a shared object (libpietest.so), loaded at SHLIB_BASE by kernel.
 * The main PIE binary calls test_add() through PLT — resolved lazily by ld.so. */

int test_add(int a, int b) {
    return a + b;
}
