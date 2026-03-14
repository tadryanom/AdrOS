// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "libgen.h"
#include "string.h"

char* basename(char* path) {
    if (!path || !*path) return ".";
    size_t len = strlen(path);
    /* Remove trailing slashes */
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    char* p = strrchr(path, '/');
    return p ? p + 1 : path;
}

char* dirname(char* path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t len = strlen(path);
    /* Remove trailing slashes */
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    char* p = strrchr(path, '/');
    if (!p) return dot;
    if (p == path) { path[1] = '\0'; return path; }
    *p = '\0';
    return path;
}
