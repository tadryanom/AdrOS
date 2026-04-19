// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS mount utility — mount filesystems or display mounts */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <errno.h>

static void show_mounts(void) {
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
}

int main(int argc, char** argv) {
    if (argc < 2) {
        show_mounts();
        return 0;
    }

    const char* fstype = "diskfs";
    const char* device = NULL;
    const char* mountpoint = NULL;

    /* Parse options first, then collect positional args */
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            fstype = argv[++i];
        } else if (!device) {
            device = argv[i];
        } else if (!mountpoint) {
            mountpoint = argv[i];
        }
    }

    if (!device || !mountpoint) {
        fprintf(stderr, "usage: mount [-t fstype] device mountpoint\n");
        return 1;
    }

    int rc = mount(device, mountpoint, fstype, 0, NULL);
    if (rc < 0) {
        fprintf(stderr, "mount: mounting %s on %s failed: %s\n", device, mountpoint, strerror(errno));
        return 1;
    }
    return 0;
}
