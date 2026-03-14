// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "glob.h"
#include "fnmatch.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "dirent.h"
#include "errno.h"

static int glob_add(glob_t* g, const char* path) {
    if (g->gl_pathc + 1 >= g->__alloc) {
        size_t newalloc = g->__alloc ? g->__alloc * 2 : 16;
        char** nv = (char**)realloc(g->gl_pathv, newalloc * sizeof(char*));
        if (!nv) return GLOB_NOSPACE;
        g->gl_pathv = nv;
        g->__alloc = newalloc;
    }
    g->gl_pathv[g->gl_pathc] = strdup(path);
    if (!g->gl_pathv[g->gl_pathc]) return GLOB_NOSPACE;
    g->gl_pathc++;
    g->gl_pathv[g->gl_pathc] = (void*)0;
    return 0;
}

int glob(const char* pattern, int flags,
         int (*errfunc)(const char*, int), glob_t* pglob) {
    (void)errfunc;

    if (!(flags & GLOB_APPEND)) {
        pglob->gl_pathc = 0;
        pglob->gl_pathv = (void*)0;
        pglob->__alloc = 0;
    }

    /* Split pattern into directory + basename */
    char dir[256], base[256];
    const char* last_slash = strrchr(pattern, '/');
    if (last_slash) {
        size_t dlen = (size_t)(last_slash - pattern);
        if (dlen == 0) dlen = 1;
        if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
        memcpy(dir, pattern, dlen);
        dir[dlen] = '\0';
        strncpy(base, last_slash + 1, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
    } else {
        strcpy(dir, ".");
        strncpy(base, pattern, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
    }

    /* Open directory and match entries */
    int dfd = open(dir, 0);
    if (dfd < 0) {
        if (flags & GLOB_NOCHECK)
            return glob_add(pglob, pattern);
        return GLOB_ABORTED;
    }

    struct dirent de;
    char path[512];
    int found = 0;

    while (getdents(dfd, &de, sizeof(de)) > 0) {
        if (de.d_name[0] == '\0') continue;
        if (fnmatch(base, de.d_name, 0) == 0) {
            if (last_slash) {
                snprintf(path, sizeof(path), "%s/%s", dir, de.d_name);
            } else {
                strncpy(path, de.d_name, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            }
            int rc = glob_add(pglob, path);
            if (rc) { close(dfd); return rc; }
            found++;
        }
    }

    close(dfd);

    if (found == 0) {
        if (flags & GLOB_NOCHECK)
            return glob_add(pglob, pattern);
        return GLOB_NOMATCH;
    }

    if (!(flags & GLOB_NOSORT) && pglob->gl_pathc > 1) {
        /* Simple insertion sort */
        for (size_t i = 1; i < pglob->gl_pathc; i++) {
            char* tmp = pglob->gl_pathv[i];
            size_t j = i;
            while (j > 0 && strcmp(pglob->gl_pathv[j - 1], tmp) > 0) {
                pglob->gl_pathv[j] = pglob->gl_pathv[j - 1];
                j--;
            }
            pglob->gl_pathv[j] = tmp;
        }
    }

    return 0;
}

void globfree(glob_t* pglob) {
    if (!pglob || !pglob->gl_pathv) return;
    for (size_t i = 0; i < pglob->gl_pathc; i++)
        free(pglob->gl_pathv[i]);
    free(pglob->gl_pathv);
    pglob->gl_pathv = (void*)0;
    pglob->gl_pathc = 0;
    pglob->__alloc = 0;
}
