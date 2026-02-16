// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* Shared library function for PLT/GOT lazy binding test.
 * Compiled as a shared object (libpietest.so), loaded at SHLIB_BASE by kernel.
 * The main PIE binary calls test_add() through PLT — resolved lazily by ld.so. */

int test_add(int a, int b) {
    return a + b;
}
