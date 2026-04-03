// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
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
    /* Count args by walking the va_list manually via stack pointers.
     * On i386, args are pushed right-to-left on the stack.
     * We'll use a simple approach: max 32 args. */
    const char* args[33];
    const char** p = &arg;
    int i = 0;
    while (*p && i < 32) {
        args[i++] = *p;
        p++;
    }
    args[i] = (const char*)0;
    return execvp(file, (char* const*)args);
}

int execl(const char* path, const char* arg, ...) {
    const char* args[33];
    const char** p = &arg;
    int i = 0;
    while (*p && i < 32) {
        args[i++] = *p;
        p++;
    }
    args[i] = (const char*)0;
    return execve(path, (char* const*)args, environ);
}
