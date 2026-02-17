// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS mount utility — display mounted filesystems */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    /* Read /proc/mounts if available, otherwise show static info */
    int fd = open("/proc/mounts", O_RDONLY);
    if (fd >= 0) {
        char buf[1024];
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, (size_t)n);
        close(fd);
    } else {
        printf("tmpfs on / type overlayfs (rw)\n");
        printf("devfs on /dev type devfs (rw)\n");
        printf("procfs on /proc type procfs (ro)\n");
    }
    return 0;
}
