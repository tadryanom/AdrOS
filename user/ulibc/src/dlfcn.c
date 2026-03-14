// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "dlfcn.h"
#include "syscall.h"
#include "errno.h"

static char _dlerror_buf[64];
static int  _dlerror_set;

void* dlopen(const char* filename, int flags) {
    int ret = _syscall2(SYS_DLOPEN, (int)filename, flags);
    if (ret < 0 && ret > -4096) {
        _dlerror_set = 1;
        errno = -ret;
        return (void*)0;
    }
    return (void*)ret;
}

void* dlsym(void* handle, const char* symbol) {
    int ret = _syscall2(SYS_DLSYM, (int)handle, (int)symbol);
    if (ret < 0 && ret > -4096) {
        _dlerror_set = 1;
        errno = -ret;
        return (void*)0;
    }
    return (void*)ret;
}

int dlclose(void* handle) {
    return __syscall_ret(_syscall1(SYS_DLCLOSE, (int)handle));
}

char* dlerror(void) {
    if (_dlerror_set) {
        _dlerror_set = 0;
        /* minimal message */
        _dlerror_buf[0] = 'd'; _dlerror_buf[1] = 'l';
        _dlerror_buf[2] = ' '; _dlerror_buf[3] = 'e';
        _dlerror_buf[4] = 'r'; _dlerror_buf[5] = 'r';
        _dlerror_buf[6] = 'o'; _dlerror_buf[7] = 'r';
        _dlerror_buf[8] = '\0';
        return _dlerror_buf;
    }
    return (void*)0;
}
