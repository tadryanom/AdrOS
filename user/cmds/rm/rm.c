// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS rm utility */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

static int rflag = 0;   /* -r: recursive */
static int fflag = 0;   /* -f: force (no errors) */
static int dflag = 0;   /* -d: remove empty directories */

static int rm_recursive(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        if (!fflag) fprintf(stderr, "rm: cannot stat '%s'\n", path);
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        int r = unlink(path);
        if (r < 0 && !fflag) fprintf(stderr, "rm: cannot remove '%s'\n", path);
        return r;
    }
    /* Directory: remove contents first */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (!fflag) fprintf(stderr, "rm: cannot open directory '%s'\n", path);
        return -1;
    }
    char dbuf[2048];
    int rc;
    while ((rc = getdents(fd, dbuf, sizeof(dbuf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(dbuf + off);
            if (d->d_reclen == 0) break;
            if (d->d_name[0] == '.' && (d->d_name[1] == '\0' ||
                (d->d_name[1] == '.' && d->d_name[2] == '\0'))) {
                off += d->d_reclen;
                continue;
            }
            char child[512];
            size_t plen = strlen(path);
            if (plen > 0 && path[plen - 1] == '/')
                snprintf(child, sizeof(child), "%s%s", path, d->d_name);
            else
                snprintf(child, sizeof(child), "%s/%s", path, d->d_name);
            rm_recursive(child);
            off += d->d_reclen;
        }
    }
    close(fd);
    int r = rmdir(path);
    if (r < 0 && !fflag) fprintf(stderr, "rm: cannot remove directory '%s'\n", path);
    return r;
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }

    int start = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'r' || *f == 'R') rflag = 1;
                else if (*f == 'f') fflag = 1;
                else if (*f == 'd') dflag = 1;
                else {
                    fprintf(stderr, "rm: invalid option -- '%c'\n", *f);
                    return 1;
                }
                f++;
            }
            start = i + 1;
        } else {
            break;
        }
    }

    int rc = 0;
    for (int i = start; i < argc; i++) {
        if (rflag) {
            if (rm_recursive(argv[i]) < 0) rc = 1;
        } else {
            int r = unlink(argv[i]);
            if (r < 0 && dflag) {
                r = rmdir(argv[i]);
            }
            if (r < 0 && !fflag) {
                fprintf(stderr, "rm: cannot remove '%s'\n", argv[i]);
                rc = 1;
            }
        }
    }
    return rc;
}
