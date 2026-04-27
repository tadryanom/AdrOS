// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS mv utility */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mv <source> <dest>\n");
        return 1;
    }

    /* Try rename first (same filesystem) */
    if (rename(argv[1], argv[2]) == 0)
        return 0;

    /* Fallback: copy + unlink */
    int src = open(argv[1], O_RDONLY);
    if (src < 0) {
        fprintf(stderr, "mv: cannot open '%s'\n", argv[1]);
        return 1;
    }

    struct stat src_st;
    int src_mode = 0644;
    if (fstat(src, &src_st) == 0) src_mode = (int)src_st.st_mode;
    int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, (mode_t)src_mode);
    if (dst < 0) {
        fprintf(stderr, "mv: cannot create '%s'\n", argv[2]);
        close(src);
        return 1;
    }

    char buf[4096];
    int r;
    while ((r = read(src, buf, sizeof(buf))) > 0) {
        if (write(dst, buf, (size_t)r) != r) {
            fprintf(stderr, "mv: write error\n");
            close(src); close(dst);
            return 1;
        }
    }

    close(src);
    close(dst);
    unlink(argv[1]);
    return 0;
}
