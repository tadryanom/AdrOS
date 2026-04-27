// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS df utility — display filesystem disk space usage */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>

static int hflag = 0;  /* -h: human-readable */
static int Tflag = 0;  /* -T: print filesystem type */

static void format_size(unsigned long kbytes, char* buf, size_t bufsz) {
    if (!hflag) {
        snprintf(buf, bufsz, "%lu", kbytes);
        return;
    }
    if (kbytes >= 1024UL * 1024 * 1024)
        snprintf(buf, bufsz, "%.1fT", (double)kbytes / (1024.0 * 1024 * 1024));
    else if (kbytes >= 1024UL * 1024)
        snprintf(buf, bufsz, "%.1fG", (double)kbytes / (1024.0 * 1024));
    else if (kbytes >= 1024UL)
        snprintf(buf, bufsz, "%.1fM", (double)kbytes / 1024.0);
    else
        snprintf(buf, bufsz, "%luK", kbytes);
}

int main(int argc, char** argv) {
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-h") == 0) hflag = 1;
        else if (strcmp(argv[argi], "-T") == 0) Tflag = 1;
        else if (strcmp(argv[argi], "--") == 0) { argi++; break; }
        else { fprintf(stderr, "df: invalid option '%s'\n", argv[argi]); return 1; }
        argi++;
    }

    /* Print header */
    if (Tflag)
        printf("Filesystem     Type   1K-blocks      Used Available Use%% Mounted on\n");
    else
        printf("Filesystem     1K-blocks      Used Available Use%% Mounted on\n");

    /* Read /proc/mounts */
    int fd = open("/proc/mounts", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "df: cannot open /proc/mounts\n");
        return 1;
    }

    char buf[2048];
    int total = 0, n;
    while ((n = read(fd, buf + total, (size_t)(sizeof(buf) - 1 - total))) > 0)
        total += n;
    buf[total] = '\0';
    close(fd);

    /* Parse each line: dev dir type ... */
    char* line = buf;
    while (line && *line) {
        char* nl = line;
        while (*nl && *nl != '\n') nl++;
        int has_nl = (*nl == '\n');
        if (has_nl) *nl = '\0';

        /* Tokenize: dev dir type ... */
        char* tokens[8];
        int ntok = 0;
        char* p = line;
        while (*p && ntok < 8) {
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;
            tokens[ntok++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }

        if (ntok >= 2) {
            const char* dev  = tokens[0];
            const char* dir  = tokens[1];
            const char* type = (ntok >= 3) ? tokens[2] : "unknown";

            struct statvfs fsbuf;
            if (statvfs(dir, &fsbuf) == 0) {
                unsigned long bsize = fsbuf.f_frsize ? fsbuf.f_frsize : fsbuf.f_bsize;
                unsigned long total_kb = (fsbuf.f_blocks * bsize) / 1024;
                unsigned long free_kb  = (fsbuf.f_bfree * bsize) / 1024;
                unsigned long used_kb  = total_kb - free_kb;
                unsigned long avail_kb = (fsbuf.f_bavail * bsize) / 1024;
                int pct = (total_kb > 0) ? (int)((used_kb * 100) / total_kb) : 0;

                char stotal[16], sused[16], savail[16];
                if (hflag) {
                    format_size(total_kb, stotal, sizeof(stotal));
                    format_size(used_kb, sused, sizeof(sused));
                    format_size(avail_kb, savail, sizeof(savail));
                    if (Tflag)
                        printf("%-14s %-6s %6s %6s %6s %3d%% %s\n",
                               dev, type, stotal, sused, savail, pct, dir);
                    else
                        printf("%-14s %6s %6s %6s %3d%% %s\n",
                               dev, stotal, sused, savail, pct, dir);
                } else {
                    if (Tflag)
                        printf("%-14s %-6s %10lu %10lu %10lu %3d%% %s\n",
                               dev, type, total_kb, used_kb, avail_kb, pct, dir);
                    else
                        printf("%-14s %10lu %10lu %10lu %3d%% %s\n",
                               dev, total_kb, used_kb, avail_kb, pct, dir);
                }
            }
        }

        line = has_nl ? nl + 1 : nl;
    }

    return 0;
}
