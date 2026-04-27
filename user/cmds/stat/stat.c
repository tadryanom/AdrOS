// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS stat utility — display file status */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

static void format_time(time_t t, char* buf, size_t bufsz) {
    strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", localtime(&t));
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: stat FILE...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) {
            fprintf(stderr, "stat: cannot stat '%s'\n", argv[i]);
            rc = 1;
            continue;
        }

        /* File type */
        const char* type = "unknown";
        if (S_ISREG(st.st_mode)) type = "regular file";
        else if (S_ISDIR(st.st_mode)) type = "directory";
        else if (S_ISCHR(st.st_mode)) type = "character device";
        else if (S_ISBLK(st.st_mode)) type = "block device";
        else if (S_ISFIFO(st.st_mode)) type = "fifo";
        else if (S_ISLNK(st.st_mode)) type = "symbolic link";

        /* UID/GID names */
        char owner[32], group[32];
        struct passwd* pw = getpwuid(st.st_uid);
        if (pw) snprintf(owner, sizeof(owner), "%s (%u)", pw->pw_name, (unsigned)st.st_uid);
        else snprintf(owner, sizeof(owner), "(%u)", (unsigned)st.st_uid);
        struct group* gr = getgrgid(st.st_gid);
        if (gr) snprintf(group, sizeof(group), "%s (%u)", gr->gr_name, (unsigned)st.st_gid);
        else snprintf(group, sizeof(group), "(%u)", (unsigned)st.st_gid);

        /* Format times */
        char mtime_str[32], atime_str[32], ctime_str[32];
        format_time((time_t)st.st_mtime, mtime_str, sizeof(mtime_str));
        format_time((time_t)st.st_atime, atime_str, sizeof(atime_str));
        format_time((time_t)st.st_ctime, ctime_str, sizeof(ctime_str));

        printf("  File: %s\n", argv[i]);
        printf("  Size: %u\t\tBlocks: %u\t\t%s\n", (unsigned)st.st_size, (unsigned)st.st_blocks, type);
        printf("  Inode: %u\t\tLinks: %u\n", (unsigned)st.st_ino, (unsigned)st.st_nlink);
        printf("  Access: (%04o/%s)\tUid: %s\tGid: %s\n",
               (unsigned)(st.st_mode & 07777), type, owner, group);
        printf("  Access: %s\n", atime_str);
        printf("  Modify: %s\n", mtime_str);
        printf("  Change: %s\n", ctime_str);
    }
    return rc;
}
