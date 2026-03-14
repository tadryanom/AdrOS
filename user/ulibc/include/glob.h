// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_GLOB_H
#define ULIBC_GLOB_H

#include <stddef.h>

#define GLOB_ERR      0x01
#define GLOB_MARK     0x02
#define GLOB_NOSORT   0x04
#define GLOB_DOOFFS   0x08
#define GLOB_NOCHECK  0x10
#define GLOB_APPEND   0x20
#define GLOB_NOESCAPE 0x40

#define GLOB_NOSPACE  1
#define GLOB_ABORTED  2
#define GLOB_NOMATCH  3

typedef struct {
    size_t  gl_pathc;
    char**  gl_pathv;
    size_t  gl_offs;
    /* internal */
    size_t  __alloc;
} glob_t;

int  glob(const char* pattern, int flags,
          int (*errfunc)(const char*, int), glob_t* pglob);
void globfree(glob_t* pglob);

#endif
