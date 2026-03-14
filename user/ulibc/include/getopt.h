// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_GETOPT_H
#define ULIBC_GETOPT_H

extern char* optarg;
extern int   optind;
extern int   opterr;
extern int   optopt;

int getopt(int argc, char* const argv[], const char* optstring);

struct option {
    const char* name;
    int         has_arg;
    int*        flag;
    int         val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

int getopt_long(int argc, char* const argv[], const char* optstring,
                const struct option* longopts, int* longindex);

#endif
