// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS dd utility — convert and copy a file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static long parse_size(const char* s) {
    long v = strtol(s, NULL, 10);
    int len = (int)strlen(s);
    if (len > 0) {
        char suf = s[len - 1];
        if (suf == 'k' || suf == 'K') v *= 1024L;
        else if (suf == 'm' || suf == 'M') v *= 1024L * 1024L;
    }
    return v;
}

#define CONV_UCASE   0x01
#define CONV_LCASE   0x02
#define CONV_SWAB    0x04
#define CONV_NOERROR 0x08
#define CONV_SYNC    0x10
#define CONV_TRUNC   0x20

int main(int argc, char** argv) {
    const char* inf = NULL;
    const char* outf = NULL;
    long bs = 512;
    int count = -1;
    int skip = 0;
    int seek_val = 0;
    unsigned int conv = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "if=", 3) == 0) inf = argv[i] + 3;
        else if (strncmp(argv[i], "of=", 3) == 0) outf = argv[i] + 3;
        else if (strncmp(argv[i], "bs=", 3) == 0) bs = parse_size(argv[i] + 3);
        else if (strncmp(argv[i], "count=", 6) == 0) count = atoi(argv[i] + 6);
        else if (strncmp(argv[i], "skip=", 5) == 0) skip = atoi(argv[i] + 5);
        else if (strncmp(argv[i], "seek=", 5) == 0) seek_val = atoi(argv[i] + 5);
        else if (strncmp(argv[i], "conv=", 5) == 0) {
            const char* c = argv[i] + 5;
            char ccopy[64];
            strncpy(ccopy, c, sizeof(ccopy) - 1);
            ccopy[sizeof(ccopy) - 1] = '\0';
            char* save = ccopy;
            char* tok;
            while ((tok = save) != NULL) {
                char* comma = strchr(save, ',');
                if (comma) { *comma = '\0'; save = comma + 1; } else save = NULL;
                if (strcmp(tok, "ucase") == 0) conv |= CONV_UCASE;
                else if (strcmp(tok, "lcase") == 0) conv |= CONV_LCASE;
                else if (strcmp(tok, "swab") == 0) conv |= CONV_SWAB;
                else if (strcmp(tok, "noerror") == 0) conv |= CONV_NOERROR;
                else if (strcmp(tok, "sync") == 0) conv |= CONV_SYNC;
                else if (strcmp(tok, "trunc") == 0) conv |= CONV_TRUNC;
            }
        }
    }

    int ifd = STDIN_FILENO;
    int ofd = STDOUT_FILENO;

    if (inf) {
        ifd = open(inf, O_RDONLY);
        if (ifd < 0) { fprintf(stderr, "dd: cannot open '%s'\n", inf); return 1; }
    }
    if (outf) {
        int oflags = O_WRONLY | O_CREAT | O_TRUNC;
        ofd = open(outf, oflags, 0644);
        if (ofd < 0) { fprintf(stderr, "dd: cannot open '%s'\n", outf); return 1; }
    }

    /* Apply skip (input) */
    if (skip > 0 && ifd != STDIN_FILENO) {
        lseek(ifd, (off_t)(skip * bs), SEEK_SET);
    }

    /* Apply seek (output) */
    if (seek_val > 0 && ofd != STDOUT_FILENO) {
        lseek(ofd, (off_t)(seek_val * bs), SEEK_SET);
    }

    if (bs > 4096) bs = 4096;
    char buf[4096];
    int blocks = 0, partial = 0, total = 0;

    while (count < 0 || blocks + partial < count) {
        int n = read(ifd, buf, (size_t)bs);
        if (n < 0) {
            if (conv & CONV_NOERROR) continue;
            break;
        }
        if (n == 0) break;

        /* Pad with spaces if sync and partial block */
        if (conv & CONV_SYNC && n < bs) {
            memset(buf + n, ' ', (size_t)(bs - n));
            n = bs;
        }

        /* Apply conversions */
        if (conv & CONV_UCASE) {
            for (int i = 0; i < n; i++)
                if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] = (char)(buf[i] - 32);
        }
        if (conv & CONV_LCASE) {
            for (int i = 0; i < n; i++)
                if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] = (char)(buf[i] + 32);
        }
        if (conv & CONV_SWAB) {
            for (int i = 0; i + 1 < n; i += 2) {
                char tmp = buf[i]; buf[i] = buf[i+1]; buf[i+1] = tmp;
            }
        }

        write(ofd, buf, (size_t)n);
        total += n;
        if (n == bs) blocks++;
        else partial++;
    }

    fprintf(stderr, "%d+%d records in\n%d+%d records out\n%d bytes copied\n",
            blocks, partial, blocks, partial, total);

    if (inf) close(ifd);
    if (outf) close(ofd);
    return 0;
}
