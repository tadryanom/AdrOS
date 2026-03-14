// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "getopt.h"
#include "string.h"
#include <stddef.h>

char* optarg = (char*)0;
int   optind = 1;
int   opterr = 1;
int   optopt = '?';

static int _optpos = 0;  /* position within current argv element */

int getopt(int argc, char* const argv[], const char* optstring) {
    if (optind >= argc || !argv[optind]) return -1;

    const char* arg = argv[optind];

    /* Skip non-options */
    if (arg[0] != '-' || arg[1] == '\0') return -1;
    if (arg[1] == '-' && arg[2] == '\0') { optind++; return -1; } /* "--" */

    int pos = _optpos ? _optpos : 1;
    int c = arg[pos];
    _optpos = 0;

    const char* match = strchr(optstring, c);
    if (!match) {
        optopt = c;
        if (arg[pos + 1]) _optpos = pos + 1;
        else optind++;
        return '?';
    }

    if (match[1] == ':') {
        /* Option requires argument */
        if (arg[pos + 1]) {
            optarg = (char*)&arg[pos + 1];
            optind++;
        } else if (optind + 1 < argc) {
            optarg = argv[optind + 1];
            optind += 2;
        } else {
            optopt = c;
            optind++;
            return (optstring[0] == ':') ? ':' : '?';
        }
    } else {
        optarg = (char*)0;
        if (arg[pos + 1]) {
            _optpos = pos + 1;
        } else {
            optind++;
        }
    }

    return c;
}

int getopt_long(int argc, char* const argv[], const char* optstring,
                const struct option* longopts, int* longindex) {
    if (optind >= argc || !argv[optind]) return -1;

    const char* arg = argv[optind];

    /* Handle long options: --name or --name=value */
    if (arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
        const char* name = arg + 2;
        const char* eq = strchr(name, '=');
        size_t nlen = eq ? (size_t)(eq - name) : strlen(name);

        for (int i = 0; longopts[i].name; i++) {
            if (strncmp(longopts[i].name, name, nlen) == 0 &&
                longopts[i].name[nlen] == '\0') {
                if (longindex) *longindex = i;

                if (longopts[i].has_arg == required_argument || longopts[i].has_arg == optional_argument) {
                    if (eq) {
                        optarg = (char*)(eq + 1);
                    } else if (longopts[i].has_arg == required_argument && optind + 1 < argc) {
                        optarg = argv[optind + 1];
                        optind++;
                    } else if (longopts[i].has_arg == required_argument) {
                        optind++;
                        return '?';
                    } else {
                        optarg = (char*)0;
                    }
                } else {
                    optarg = (char*)0;
                }

                optind++;
                if (longopts[i].flag) {
                    *longopts[i].flag = longopts[i].val;
                    return 0;
                }
                return longopts[i].val;
            }
        }
        optind++;
        return '?';
    }

    /* Fall back to short option parsing */
    return getopt(argc, argv, optstring);
}
