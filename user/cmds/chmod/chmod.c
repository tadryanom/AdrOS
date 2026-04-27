// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS chmod utility — change file mode bits */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static unsigned int parse_symbolic(const char* mode, unsigned int old) {
    /* Parse symbolic mode: [ugoa...][+-=][rwxst...][,...] */
    unsigned int result = old;
    const char* p = mode;
    while (*p) {
        /* Parse who */
        unsigned int who = 0;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            switch (*p) {
            case 'u': who |= 04700; break; /* user: setuid + user bits */
            case 'g': who |= 02070; break; /* group: setgid + group bits */
            case 'o': who |= 00007; break; /* other bits */
            case 'a': who |= 06777; break; /* all */
            }
            p++;
        }
        if (who == 0) who = 06777; /* default: all */

        /* Parse operation */
        while (*p) {
            char op = *p;
            if (op != '+' && op != '-' && op != '=') break;
            p++;

            /* Parse permissions */
            unsigned int perm = 0;
            while (*p == 'r' || *p == 'w' || *p == 'x' ||
                   *p == 's' || *p == 't') {
                switch (*p) {
                case 'r': perm |= 0444; break;
                case 'w': perm |= 0222; break;
                case 'x': perm |= 0111; break;
                case 's': perm |= 06000; break; /* setuid+setgid */
                case 't': perm |= 01000; break; /* sticky */
                }
                p++;
            }

            /* Apply perm mask to who */
            unsigned int masked = perm & who;

            if (op == '+') {
                result |= masked;
            } else if (op == '-') {
                result &= ~masked;
            } else { /* = */
                result &= ~who;  /* clear who bits */
                result |= masked;
            }

            /* Check for comma separator or next operation */
            if (*p == ',') { p++; break; }
        }
    }
    return result & 07777;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chmod MODE FILE...\n");
        return 1;
    }

    const char* mode_str = argv[1];
    unsigned int mode;

    /* Check if symbolic or octal */
    int is_symbolic = 0;
    for (int i = 0; mode_str[i]; i++) {
        char c = mode_str[i];
        if (c == '+' || c == '-' || c == '=' || c == 'u' || c == 'g' ||
            c == 'o' || c == 'a' || c == 'r' || c == 'w' || c == 'x' ||
            c == 's' || c == 't' || c == ',') {
            is_symbolic = 1;
            break;
        }
    }

    if (is_symbolic) {
        for (int i = 2; i < argc; i++) {
            struct stat st;
            if (stat(argv[i], &st) < 0) {
                fprintf(stderr, "chmod: cannot access '%s'\n", argv[i]);
                continue;
            }
            mode = parse_symbolic(mode_str, (unsigned int)st.st_mode);
            if (chmod(argv[i], (mode_t)mode) < 0)
                fprintf(stderr, "chmod: cannot change mode of '%s'\n", argv[i]);
        }
    } else {
        mode = (unsigned int)strtol(mode_str, NULL, 8);
        for (int i = 2; i < argc; i++) {
            if (chmod(argv[i], (mode_t)mode) < 0)
                fprintf(stderr, "chmod: cannot change mode of '%s'\n", argv[i]);
        }
    }

    return 0;
}
