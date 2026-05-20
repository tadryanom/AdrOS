// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
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
        char buf[2048];
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, (size_t)n);
        close(fd);
    } else {
        fprintf(stderr, "mount: /proc/mounts not available\n");
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
    unsigned long mountflags = 0;

    /* Parse options first, then collect positional args */
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            fstype = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            i++;
            /* Parse comma-separated options */
            const char* o = argv[i];
            while (*o) {
                if (strncmp(o, "ro", 2) == 0 && (o[2] == ',' || o[2] == '\0')) {
                    mountflags |= MS_RDONLY;
                    o += 2;
                } else if (strncmp(o, "remount", 7) == 0 && (o[7] == ',' || o[7] == '\0')) {
                    mountflags |= MS_REMOUNT;
                    o += 7;
                } else if (strncmp(o, "rw", 2) == 0 && (o[2] == ',' || o[2] == '\0')) {
                    o += 2;
                } else {
                    /* Skip unknown option */
                    while (*o && *o != ',') o++;
                }
                if (*o == ',') o++;
            }
        } else if (!device) {
            device = argv[i];
        } else if (!mountpoint) {
            mountpoint = argv[i];
        }
    }

    if (!mountpoint) {
        fprintf(stderr, "usage: mount [-t fstype] [-o options] device mountpoint\n");
        return 1;
    }

    if (!device) device = "none";

    int rc = mount(device, mountpoint, fstype, mountflags, NULL);
    if (rc < 0) {
        fprintf(stderr, "mount: mounting %s on %s failed: %s\n", device, mountpoint, strerror(errno));
        return 1;
    }
    return 0;
}
