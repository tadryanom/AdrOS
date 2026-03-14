// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "stdlib.h"
#include "signal.h"
#include "unistd.h"

static void (*_atexit_funcs[32])(void);
static int _atexit_count = 0;

int atexit(void (*func)(void)) {
    if (_atexit_count >= 32) return -1;
    _atexit_funcs[_atexit_count++] = func;
    return 0;
}

void exit(int status) {
    /* Call atexit handlers in reverse order */
    for (int i = _atexit_count - 1; i >= 0; i--) {
        if (_atexit_funcs[i]) _atexit_funcs[i]();
    }
    _exit(status);
}

void abort(void) {
    raise(SIGABRT);
    _exit(127);
}
