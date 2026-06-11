// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/*
 * Early /init for root filesystem handoff
 *
 * This program runs from the initrd before the final /sbin/init.
 * 
 * Milestone C: Full pivot_root implementation
 * - Move virtual filesystems (/dev, /proc, /tmp) to /newroot
 * - Perform pivot_root to make /newroot the new root
 * - Unmount old root
 * - Execute /sbin/init from the new root
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>

/* MS_MOVE is not defined in ulibc, define it manually */
#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#define NEWROOT "/newroot"
#define OLDROOT "/oldroot"

static int mkdir_p(const char* path) {
    char tmp[256];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
                perror("mkdir_p");
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
        perror("mkdir_p");
        return -1;
    }

    return 0;
}

static int is_mounted(const char* path) {
    FILE* fp = fopen("/proc/mounts", "r");
    if (!fp) {
        return 0;
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        char mountpoint[128];
        if (sscanf(line, "%*s %127s", mountpoint) == 1) {
            if (strcmp(mountpoint, path) == 0) {
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found;
}

static int mount_virtual_fs(const char* fstype, const char* target) {
    if (is_mounted(target)) {
        printf("[init] %s already mounted on %s\n", fstype, target);
        return 0;
    }

    if (mkdir_p(target) < 0) {
        return -1;
    }

    printf("[init] mounting %s on %s\n", fstype, target);
    if (mount("none", target, fstype, 0, NULL) < 0) {
        perror("mount");
        return -1;
    }

    return 0;
}

static int move_mount(const char* src, const char* dst) {
    printf("[init] moving mount from %s to %s\n", src, dst);

    if (mkdir_p(dst) < 0) {
        return -1;
    }

    /* Linux uses MS_MOVE flag for mount --move */
    if (mount(src, dst, NULL, MS_MOVE, NULL) < 0) {
        perror("mount --move");
        return -1;
    }

    return 0;
}

int main(int argc __attribute__((unused)), char** argv) {
    printf("[init] early userspace init starting\n");

    /* Check if /newroot is already mounted by kernel */
    if (!is_mounted(NEWROOT)) {
        printf("[init] /newroot not mounted by kernel, nothing to do\n");
        printf("[init] falling back to /sbin/init\n");
        execve("/sbin/init", argv, environ);
        perror("execve /sbin/init");
        return 1;
    }

    printf("[init] /newroot is mounted, preparing handoff\n");

    /* Create /oldroot for pivot_root */
    if (mkdir(OLDROOT, 0755) < 0 && errno != EEXIST) {
        perror("mkdir /oldroot");
        return 1;
    }

    /* Move virtual filesystems to /newroot if they exist on old root */
    const char* virtual_mounts[] = { "/dev", "/proc", "/tmp", NULL };
    for (int i = 0; virtual_mounts[i]; i++) {
        char src[128];
        char dst[128];

        snprintf(src, sizeof(src), "%s", virtual_mounts[i]);
        snprintf(dst, sizeof(dst), "%s%s", NEWROOT, virtual_mounts[i]);

        if (is_mounted(src)) {
            if (move_mount(src, dst) < 0) {
                printf("[init] warning: failed to move %s, will remount\n", src);
                /* Fallback: mount directly on new root */
                const char* fstype = NULL;
                if (strcmp(src, "/dev") == 0) fstype = "devfs";
                else if (strcmp(src, "/proc") == 0) fstype = "procfs";
                else if (strcmp(src, "/tmp") == 0) fstype = "tmpfs";

                if (fstype) {
                    mount_virtual_fs(fstype, dst);
                }
            }
        } else {
            /* Not mounted on old root, mount directly on new root */
            const char* fstype = NULL;
            if (strcmp(src, "/dev") == 0) fstype = "devfs";
            else if (strcmp(src, "/proc") == 0) fstype = "procfs";
            else if (strcmp(src, "/tmp") == 0) fstype = "tmpfs";

            if (fstype) {
                mount_virtual_fs(fstype, dst);
            }
        }
    }

    /* Perform pivot_root */
    printf("[init] performing pivot_root(%s, %s)\n", NEWROOT, OLDROOT);
    if (pivot_root(NEWROOT, OLDROOT) < 0) {
        perror("pivot_root");
        printf("[init] pivot_root failed, falling back to /sbin/init\n");
        execve("/sbin/init", argv, environ);
        return 1;
    }

    /* Change to new root */
    if (chdir("/") < 0) {
        perror("chdir /");
        return 1;
    }

    /* Unmount old root */
    printf("[init] unmounting %s\n", OLDROOT);
    if (umount2(OLDROOT, 0) < 0) {
        perror("umount2 /oldroot");
        printf("[init] warning: failed to unmount old root\n");
    }

    /* Execute final init from new root */
    printf("[init] executing /sbin/init from new root\n");
    execve("/sbin/init", argv, environ);
    perror("execve /sbin/init");

    return 1;
}
