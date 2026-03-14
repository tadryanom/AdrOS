// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include <stddef.h>

extern char** __environ;

static char* _env_storage[128];
static int _env_owned = 0;

static void _ensure_own_environ(void) {
    if (_env_owned) return;
    int count = 0;
    if (__environ) {
        for (; __environ[count]; count++) {
            if (count >= 126) break;
            _env_storage[count] = __environ[count];
        }
    }
    _env_storage[count] = (char*)0;
    __environ = _env_storage;
    _env_owned = 1;
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    _ensure_own_environ();

    size_t nlen = strlen(name);
    /* Check if already exists */
    for (int i = 0; __environ[i]; i++) {
        if (strncmp(__environ[i], name, nlen) == 0 && __environ[i][nlen] == '=') {
            if (!overwrite) return 0;
            /* Replace in-place */
            size_t vlen = strlen(value);
            char* buf = malloc(nlen + 1 + vlen + 1);
            if (!buf) { errno = ENOMEM; return -1; }
            memcpy(buf, name, nlen);
            buf[nlen] = '=';
            memcpy(buf + nlen + 1, value, vlen + 1);
            __environ[i] = buf;
            return 0;
        }
    }

    /* Count entries */
    int count = 0;
    while (__environ[count]) count++;
    if (count >= 126) { errno = ENOMEM; return -1; }

    size_t vlen = strlen(value);
    char* buf = malloc(nlen + 1 + vlen + 1);
    if (!buf) { errno = ENOMEM; return -1; }
    memcpy(buf, name, nlen);
    buf[nlen] = '=';
    memcpy(buf + nlen + 1, value, vlen + 1);

    __environ[count] = buf;
    __environ[count + 1] = (char*)0;
    return 0;
}

int unsetenv(const char* name) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    _ensure_own_environ();

    size_t nlen = strlen(name);
    for (int i = 0; __environ[i]; i++) {
        if (strncmp(__environ[i], name, nlen) == 0 && __environ[i][nlen] == '=') {
            /* Shift remaining entries down */
            for (int j = i; __environ[j]; j++)
                __environ[j] = __environ[j + 1];
            return 0;
        }
    }
    return 0;
}

int putenv(char* string) {
    if (!string) { errno = EINVAL; return -1; }

    _ensure_own_environ();

    char* eq = strchr(string, '=');
    if (!eq) return unsetenv(string);

    size_t nlen = (size_t)(eq - string);

    /* Replace existing or append */
    for (int i = 0; __environ[i]; i++) {
        if (strncmp(__environ[i], string, nlen + 1) == 0) {
            __environ[i] = string;
            return 0;
        }
    }

    int count = 0;
    while (__environ[count]) count++;
    if (count >= 126) { errno = ENOMEM; return -1; }
    __environ[count] = string;
    __environ[count + 1] = (char*)0;
    return 0;
}
