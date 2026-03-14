// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _FEATURES_H
#define _FEATURES_H

/* Minimal features.h for AdrOS/newlib */
#ifndef __GLIBC__
#define __GLIBC__ 2
#define __GLIBC_MINOR__ 0
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#ifndef _GNU_SOURCE
/* leave undefined unless explicitly requested */
#endif

#endif
