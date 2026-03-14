// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_REGEX_H
#define ULIBC_REGEX_H

#include <stddef.h>

/* regcomp cflags */
#define REG_EXTENDED  1
#define REG_ICASE     2
#define REG_NEWLINE   4
#define REG_NOSUB     8

/* regexec eflags */
#define REG_NOTBOL    1
#define REG_NOTEOL    2

/* Error codes */
#define REG_OK        0
#define REG_NOMATCH   1
#define REG_BADPAT    2
#define REG_ECOLLATE  3
#define REG_ECTYPE    4
#define REG_EESCAPE   5
#define REG_ESUBREG   6
#define REG_EBRACK    7
#define REG_EPAREN    8
#define REG_EBRACE    9
#define REG_BADBR    10
#define REG_ERANGE   11
#define REG_ESPACE   12
#define REG_BADRPT   13

typedef struct {
    size_t re_nsub;
    void*  _priv;    /* internal compiled pattern */
} regex_t;

typedef int regoff_t;

typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

int  regcomp(regex_t* preg, const char* pattern, int cflags);
int  regexec(const regex_t* preg, const char* string,
             size_t nmatch, regmatch_t pmatch[], int eflags);
void regfree(regex_t* preg);
size_t regerror(int errcode, const regex_t* preg,
                char* errbuf, size_t errbuf_size);

#endif
