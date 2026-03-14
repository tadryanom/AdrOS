// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_WORDEXP_H
#define ULIBC_WORDEXP_H

#include <stddef.h>

#define WRDE_DOOFFS  0x01
#define WRDE_APPEND  0x02
#define WRDE_NOCMD   0x04
#define WRDE_REUSE   0x08
#define WRDE_SHOWERR 0x10
#define WRDE_UNDEF   0x20

#define WRDE_BADCHAR 1
#define WRDE_BADVAL  2
#define WRDE_CMDSUB  3
#define WRDE_NOSPACE 4
#define WRDE_SYNTAX  5

typedef struct {
    size_t we_wordc;
    char** we_wordv;
    size_t we_offs;
} wordexp_t;

int  wordexp(const char* words, wordexp_t* pwordexp, int flags);
void wordfree(wordexp_t* pwordexp);

#endif
