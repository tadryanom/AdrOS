// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "fnmatch.h"
#include <stddef.h>

int fnmatch(const char* pattern, const char* string, int flags) {
    const char* p = pattern;
    const char* s = string;

    while (*p) {
        if (*p == '*') {
            p++;
            /* Skip consecutive stars */
            while (*p == '*') p++;
            if (!*p) return 0;  /* trailing * matches everything */
            /* Try matching rest of pattern at each position */
            while (*s) {
                if (fnmatch(p, s, flags) == 0) return 0;
                if ((flags & FNM_PATHNAME) && *s == '/') break;
                s++;
            }
            return FNM_NOMATCH;
        } else if (*p == '?') {
            if (!*s) return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && *s == '/') return FNM_NOMATCH;
            if ((flags & FNM_PERIOD) && *s == '.' && (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/')))
                return FNM_NOMATCH;
            p++; s++;
        } else if (*p == '[') {
            if (!*s) return FNM_NOMATCH;
            p++;
            int negate = 0;
            if (*p == '!' || *p == '^') { negate = 1; p++; }
            int match = 0;
            while (*p && *p != ']') {
                char lo = *p++;
                if (*p == '-' && p[1] && p[1] != ']') {
                    p++;
                    char hi = *p++;
                    if (*s >= lo && *s <= hi) match = 1;
                } else {
                    if (*s == lo) match = 1;
                }
            }
            if (*p == ']') p++;
            if (negate) match = !match;
            if (!match) return FNM_NOMATCH;
            s++;
        } else {
            /* Literal character */
            if (*p != *s) return FNM_NOMATCH;
            p++; s++;
        }
    }

    return (*s == '\0') ? 0 : FNM_NOMATCH;
}
