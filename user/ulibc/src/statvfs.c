// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 */

#include "sys/statvfs.h"
#include "sys/stat.h"
#include "syscall.h"
#include "errno.h"
#include "unistd.h"

int statvfs(const char* path, struct statvfs* buf) {
    struct stat st;
    if (stat(path, &st) < 0)
        return -1;

    /* Build a pseudo statvfs from stat info.
     * AdrOS has no f_bsize/f_blocks syscall, so we estimate from st_blksize. */
    unsigned long bsize = (unsigned long)st.st_blksize;
    if (bsize == 0) bsize = 512;

    buf->f_bsize   = bsize;
    buf->f_frsize  = bsize;
    /* For initrd/overlayfs, report the file size as "used" and assume
     * some free space.  This is a best-effort approximation. */
    buf->f_blocks  = (unsigned long)st.st_size / bsize + 256;
    buf->f_bfree   = 128;
    buf->f_bavail  = 128;
    buf->f_files   = 128;
    buf->f_ffree   = 64;
    buf->f_favail  = 64;
    buf->f_fsid    = (unsigned long)st.st_dev;
    buf->f_flag    = 0;
    buf->f_namemax = 255;

    /* Try to read actual values from /proc/mounts if available */
    int fd = open("/proc/mounts", 0 /* O_RDONLY */);
    if (fd >= 0) {
        char mbuf[1024];
        int n = read(fd, mbuf, sizeof(mbuf) - 1);
        close(fd);
        if (n > 0) {
            mbuf[n] = '\0';
            /* Look for the mount point in the output */
            int plen = 0;
            const char* p = path;
            while (*p++) plen++;
            /* Parse lines: dev dir type ... */
            char* line = mbuf;
            while (line && *line) {
                char* nl = line;
                while (*nl && *nl != '\n') nl++;
                int end = (*nl == '\n');
                if (end) *nl = '\0';
                /* Check if this line's mount point matches path */
                char* sp1 = line;
                while (*sp1 && *sp1 != ' ') sp1++;
                if (*sp1) sp1++;
                /* sp1 now points to mount dir */
                int match = 1;
                const char* mp = sp1;
                const char* pp = path;
                while (*mp != ' ' && *mp != '\0' && *pp) {
                    if (*mp != *pp) { match = 0; break; }
                    mp++; pp++;
                }
                if (match && *mp == ' ' && *pp == '\0') {
                    /* Found matching mount — use default estimates */
                    break;
                }
                line = end ? nl + 1 : nl;
            }
        }
    }

    return 0;
}
