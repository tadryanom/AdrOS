// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "signal.h"

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
    struct sigaction sa, old;
    sa.sa_handler = (uintptr_t)handler;
    sa.sa_mask = 0;
    sa.sa_flags = SA_RESTART;
    if (sigaction(signum, &sa, &old) < 0)
        return (sighandler_t)-1;  /* SIG_ERR */
    return (sighandler_t)(uintptr_t)old.sa_handler;
}
