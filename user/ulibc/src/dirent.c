// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "dirent.h"
#include "unistd.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "syscall.h"
#include "errno.h"

DIR* opendir(const char* name) {
    int fd = open(name, O_RDONLY);
    if (fd < 0) return (void*)0;
    DIR* d = (DIR*)malloc(sizeof(DIR));
    if (!d) { close(fd); errno = ENOMEM; return (void*)0; }
    d->fd  = fd;
    d->pos = 0;
    d->len = 0;
    return d;
}

static struct dirent _de_static;

struct dirent* readdir(DIR* dirp) {
    if (!dirp) return (void*)0;

    /* Refill buffer if exhausted */
    if (dirp->pos >= dirp->len) {
        int n = getdents(dirp->fd, dirp->buf, sizeof(dirp->buf));
        if (n <= 0) return (void*)0;
        dirp->len = n;
        dirp->pos = 0;
    }

    struct dirent* ent = (struct dirent*)(dirp->buf + dirp->pos);
    if (ent->d_reclen == 0) return (void*)0;
    if (dirp->pos + ent->d_reclen > dirp->len) return (void*)0;

    memset(&_de_static, 0, sizeof(_de_static));
    if (ent->d_reclen > sizeof(_de_static))
        memcpy(&_de_static, ent, sizeof(_de_static));
    else
        memcpy(&_de_static, ent, ent->d_reclen);
    _de_static.d_name[sizeof(_de_static.d_name) - 1] = '\0';
    dirp->pos += ent->d_reclen;
    return &_de_static;
}

int closedir(DIR* dirp) {
    if (!dirp) return -1;
    int r = close(dirp->fd);
    free(dirp);
    return r;
}

void rewinddir(DIR* dirp) {
    if (!dirp) return;
    lseek(dirp->fd, 0, SEEK_SET);
    dirp->pos = 0;
    dirp->len = 0;
}
