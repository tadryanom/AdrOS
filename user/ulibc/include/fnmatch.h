// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_FNMATCH_H
#define ULIBC_FNMATCH_H

#define FNM_NOMATCH    1
#define FNM_PATHNAME   (1 << 0)
#define FNM_NOESCAPE   (1 << 1)
#define FNM_PERIOD     (1 << 2)

int fnmatch(const char* pattern, const char* string, int flags);

#endif
