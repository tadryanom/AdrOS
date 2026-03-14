// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "dirent.h"
#include "unistd.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "syscall.h"
#include "errno.h"

/* AdrOS getdents returns fixed-size entries: { uint32_t ino; char name[256]; } = 260 bytes */
#define ADROS_DIRENT_SIZE 260

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

    if (dirp->pos + ADROS_DIRENT_SIZE > dirp->len) return (void*)0;

    char* ent = dirp->buf + dirp->pos;
    uint32_t ino;
    memcpy(&ino, ent, 4);
    _de_static.d_ino = ino;
    _de_static.d_reclen = ADROS_DIRENT_SIZE;
    _de_static.d_type = DT_UNKNOWN;
    strncpy(_de_static.d_name, ent + 4, 255);
    _de_static.d_name[255] = '\0';
    dirp->pos += ADROS_DIRENT_SIZE;
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
