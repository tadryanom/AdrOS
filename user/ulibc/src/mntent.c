// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "mntent.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

static struct mntent _mnt;
static char _mnt_buf[512];

FILE* setmntent(const char* filename, const char* type) {
    return fopen(filename, type);
}

struct mntent* getmntent(FILE* fp) {
    if (!fp) return NULL;

    while (fgets(_mnt_buf, (int)sizeof(_mnt_buf), fp)) {
        /* Skip comments and blank lines */
        if (_mnt_buf[0] == '#' || _mnt_buf[0] == '\n' || _mnt_buf[0] == '\0')
            continue;

        /* Remove trailing newline */
        size_t len = strlen(_mnt_buf);
        if (len > 0 && _mnt_buf[len - 1] == '\n')
            _mnt_buf[len - 1] = '\0';

        /* Parse: fsname dir type opts freq passno */
        char* p = _mnt_buf;
        _mnt.mnt_fsname = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
        while (*p == ' ' || *p == '\t') p++;

        _mnt.mnt_dir = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
        while (*p == ' ' || *p == '\t') p++;

        _mnt.mnt_type = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
        while (*p == ' ' || *p == '\t') p++;

        _mnt.mnt_opts = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
        while (*p == ' ' || *p == '\t') p++;

        _mnt.mnt_freq = atoi(p);
        while (*p && *p != ' ' && *p != '\t') p++;
        while (*p == ' ' || *p == '\t') p++;

        _mnt.mnt_passno = atoi(p);

        return &_mnt;
    }

    return NULL;
}

int addmntent(FILE* fp, const struct mntent* mnt) {
    if (!fp || !mnt) return 1;
    fprintf(fp, "%s %s %s %s %d %d\n",
            mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type,
            mnt->mnt_opts, mnt->mnt_freq, mnt->mnt_passno);
    return 0;
}

int endmntent(FILE* fp) {
    if (fp) fclose(fp);
    return 1;  /* always returns 1 per POSIX */
}

char* hasmntopt(const struct mntent* mnt, const char* opt) {
    if (!mnt || !mnt->mnt_opts || !opt) return NULL;
    return strstr(mnt->mnt_opts, opt);
}
