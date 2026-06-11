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
 * Milestone B: Basic early init - checks for /newroot and executes /sbin/init
 * Milestone C: Will add pivot_root and mount moving
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define NEWROOT "/newroot"

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

    printf("[init] /newroot is mounted by kernel\n");
    printf("[init] Milestone B: basic handoff - executing /sbin/init directly\n");
    printf("[init] (pivot_root and mount moving will be added in Milestone C)\n");

    /* For now, just execute /sbin/init from the initrd overlay */
    /* The kernel has already mounted the real root on /newroot */
    /* Future Milestone C will implement pivot_root to make /newroot the actual root */
    execve("/sbin/init", argv, environ);
    perror("execve /sbin/init");

    return 1;
}
