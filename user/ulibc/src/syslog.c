// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "syslog.h"
#include <stdarg.h>

static const char* _syslog_ident = "";
static int _syslog_facility = 0;

void openlog(const char* ident, int option, int facility) {
    (void)option;
    _syslog_ident = ident ? ident : "";
    _syslog_facility = facility;
}

void syslog(int priority, const char* format, ...) {
    (void)priority;
    (void)format;
    /* Stub: AdrOS has no syslog daemon. Messages are silently dropped. */
}

void closelog(void) {
    _syslog_ident = "";
    _syslog_facility = 0;
}
