// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"
#include <stddef.h>

extern char** environ;

int execvp(const char* file, char* const argv[]) {
    if (!file || !*file) { errno = ENOENT; return -1; }

    /* If file contains '/', use it directly */
    if (strchr(file, '/'))
        return execve(file, argv, environ);

    /* Search PATH */
    const char* path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";

    char buf[256];
    size_t flen = strlen(file);

    while (*path) {
        const char* sep = path;
        while (*sep && *sep != ':') sep++;
        size_t dlen = (size_t)(sep - path);
        if (dlen == 0) { dlen = 1; path = "."; }

        if (dlen + 1 + flen + 1 > sizeof(buf)) {
            path = *sep ? sep + 1 : sep;
            continue;
        }

        memcpy(buf, path, dlen);
        buf[dlen] = '/';
        memcpy(buf + dlen + 1, file, flen + 1);

        execve(buf, argv, environ);
        /* If ENOENT, try next; otherwise fail */
        if (errno != ENOENT) return -1;

        path = *sep ? sep + 1 : sep;
    }

    errno = ENOENT;
    return -1;
}

int execlp(const char* file, const char* arg, ...) {
    /* U04: Use va_list instead of pointer arithmetic for portability */
    const char* args[33];
    __builtin_va_list ap;
    __builtin_va_start(ap, arg);
    int i = 0;
    args[i++] = arg;
    while (i < 32) {
        const char* a = __builtin_va_arg(ap, const char*);
        if (!a) break;
        args[i++] = a;
    }
    args[i] = (const char*)0;
    __builtin_va_end(ap);
    return execvp(file, (char* const*)args);
}

int execl(const char* path, const char* arg, ...) {
    /* U04: Use va_list instead of pointer arithmetic for portability */
    const char* args[33];
    __builtin_va_list ap;
    __builtin_va_start(ap, arg);
    int i = 0;
    args[i++] = arg;
    while (i < 32) {
        const char* a = __builtin_va_arg(ap, const char*);
        if (!a) break;
        args[i++] = a;
    }
    args[i] = (const char*)0;
    __builtin_va_end(ap);
    return execve(path, (char* const*)args, environ);
}
