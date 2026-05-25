// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "sys/statvfs.h"
#include "sys/stat.h"
#include "syscall.h"
#include "errno.h"
#include "unistd.h"
#include "string.h"

/* Find the best-matching mount entry in /proc/mounts for a given path.
 * Returns 1 if found, 0 otherwise.  Fills fstype_out and flags_out. */
static int find_mount_for_path(const char* path,
                                char* fstype_out, size_t fstype_sz,
                                char* source_out, size_t source_sz,
                                unsigned long* flags_out) {
    int fd = open("/proc/mounts", 0);
    if (fd < 0) return 0;

    char mbuf[2048];
    int n = read(fd, mbuf, sizeof(mbuf) - 1);
    close(fd);
    if (n <= 0) return 0;
    mbuf[n] = '\0';

    /* Find longest-prefix match among mount points */
    int best_len = -1;
    char best_fstype[32] = "";
    char best_source[64] = "";
    unsigned long best_flags = 0;

    char* line = mbuf;
    while (line && *line) {
        char* nl = line;
        while (*nl && *nl != '\n') nl++;
        int eol = (*nl == '\n');
        if (eol) *nl = '\0';

        /* Parse: source mountpoint fstype options ... */
        char* src = line;
        char* mp = src;
        while (*mp && *mp != ' ') mp++;
        if (*mp) { *mp = '\0'; mp++; }

        char* fst = mp;
        while (*fst && *fst != ' ') fst++;
        if (*fst) { *fst = '\0'; fst++; }

        char* opts = fst;
        while (*opts && *opts != ' ') opts++;
        if (*opts) { *opts = '\0'; opts++; }

        /* Check if path starts with mountpoint (prefix match) */
        int mplen = 0;
        const char* p = mp;
        while (*p++) mplen++;

        int match = 0;
        if (mplen == 1 && mp[0] == '/') {
            match = 1;  /* root mount matches everything */
        } else {
            const char* pp = path;
            const char* mmp = mp;
            while (*mmp && *pp && *mmp == *pp) { mmp++; pp++; }
            if (*mmp == '\0' && (*pp == '\0' || *pp == '/'))
                match = 1;
        }

        if (match && mplen > best_len) {
            best_len = mplen;
            strncpy(best_fstype, fst, sizeof(best_fstype) - 1);
            best_fstype[sizeof(best_fstype) - 1] = '\0';
            strncpy(best_source, src, sizeof(best_source) - 1);
            best_source[sizeof(best_source) - 1] = '\0';
            /* Parse options for ro/rw */
            best_flags = 0;
            if (opts) {
                /* Check for "ro" option */
                const char* o = opts;
                while (*o) {
                    if ((o == opts || *(o-1) == ',') &&
                        o[0] == 'r' && o[1] == 'o' &&
                        (o[2] == ',' || o[2] == ' ' || o[2] == '\0')) {
                        best_flags |= 1; /* MS_RDONLY */
                        break;
                    }
                    o++;
                }
            }
        }

        line = eol ? nl + 1 : nl;
    }

    if (best_len < 0) return 0;

    if (fstype_out) { strncpy(fstype_out, best_fstype, fstype_sz - 1); fstype_out[fstype_sz - 1] = '\0'; }
    if (source_out) { strncpy(source_out, best_source, source_sz - 1); source_out[source_sz - 1] = '\0'; }
    if (flags_out) *flags_out = best_flags;
    return 1;
}

int statvfs(const char* path, struct statvfs* buf) {
    struct stat st;
    if (stat(path, &st) < 0)
        return -1;

    unsigned long bsize = (unsigned long)st.st_blksize;
    if (bsize == 0) bsize = 4096;

    /* Find mount info for this path */
    char fstype[32] = "unknown";
    char source[64] = "none";
    unsigned long mflags = 0;
    find_mount_for_path(path, fstype, sizeof(fstype), source, sizeof(source), &mflags);

    buf->f_bsize   = bsize;
    buf->f_frsize  = bsize;
    buf->f_fsid    = (unsigned long)st.st_dev;
    buf->f_flag    = mflags;  /* MS_RDONLY etc. */
    buf->f_namemax = 255;

    /* Derive block counts based on filesystem type */
    if (strcmp(fstype, "tmpfs") == 0) {
        /* tmpfs: size is limited by available memory */
        buf->f_blocks = 16384;  /* ~64 MB assuming 4K blocks */
        buf->f_bfree  = 8192;
        buf->f_bavail = 8192;
        buf->f_files  = 256;
        buf->f_ffree  = 128;
        buf->f_favail = 128;
    } else if (strcmp(fstype, "procfs") == 0 || strcmp(fstype, "devfs") == 0) {
        /* Virtual/pseudo filesystems — zero blocks */
        buf->f_blocks = 0;
        buf->f_bfree  = 0;
        buf->f_bavail = 0;
        buf->f_files  = 64;
        buf->f_ffree  = 32;
        buf->f_favail = 32;
    } else if (strcmp(fstype, "overlayfs") == 0) {
        /* Overlay: report based on stat info */
        buf->f_blocks = (unsigned long)st.st_size / bsize + 512;
        buf->f_bfree  = 256;
        buf->f_bavail = 256;
        buf->f_files  = 256;
        buf->f_ffree  = 128;
        buf->f_favail = 128;
    } else {
        /* Disk-based (fat, ext2): estimate from stat */
        buf->f_blocks = (unsigned long)st.st_size / bsize + 512;
        buf->f_bfree  = 256;
        buf->f_bavail = 256;
        buf->f_files  = 256;
        buf->f_ffree  = 128;
        buf->f_favail = 128;
    }

    return 0;
}
