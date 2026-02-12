// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_STDLIB_H
#define ULIBC_STDLIB_H

#include <stddef.h>

void*   malloc(size_t size);
void    free(void* ptr);
void*   calloc(size_t nmemb, size_t size);
void*   realloc(void* ptr, size_t size);

int     atoi(const char* s);
double  atof(const char* s);
long    strtol(const char* nptr, char** endptr, int base);
char*   realpath(const char* path, char* resolved);
char*   getenv(const char* name);
int     abs(int x);
long    labs(long x);

int     system(const char* cmd);
void    exit(int status) __attribute__((noreturn));

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
