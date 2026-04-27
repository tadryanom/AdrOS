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
    /* Parse symbolic mode: [ugoa...][+-=][rwxst...][,...]
     * For each who-specifier, rwxst are mapped to that who's bit range:
     *   u+rwx → 0700,  g+rwx → 0070,  o+rwx → 0007
     *   u+s   → 4000,  g+s    → 2000,  s alone → 6000
     *   u+t   → (invalid, sticky is others-only), t → 1000
     */
    unsigned int result = old;
    const char* p = mode;
    while (*p) {
        /* Parse who — build per-who permission mapping */
        unsigned int who_bits = 0;  /* which bit positions to affect */
        int has_u = 0, has_g = 0, has_o = 0;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            switch (*p) {
            case 'u': has_u = 1; break;
            case 'g': has_g = 1; break;
            case 'o': has_o = 1; break;
            case 'a': has_u = has_g = has_o = 1; break;
            }
            p++;
        }
        if (!has_u && !has_g && !has_o) has_u = has_g = has_o = 1; /* default: all */

        /* Build who_bits from who specifiers */
        if (has_u) who_bits |= 04700; /* setuid + user rwx */
        if (has_g) who_bits |= 02070; /* setgid + group rwx */
        if (has_o) who_bits |= 00007; /* other rwx */

        /* Parse operation */
        while (*p) {
            char op = *p;
            if (op != '+' && op != '-' && op != '=') break;
            p++;

            /* Parse permissions — map to who-specific bits */
            unsigned int perm = 0;
            while (*p == 'r' || *p == 'w' || *p == 'x' ||
                   *p == 's' || *p == 't') {
                switch (*p) {
                case 'r':
                    if (has_u) perm |= 0400;
                    if (has_g) perm |= 0040;
                    if (has_o) perm |= 0004;
                    break;
                case 'w':
                    if (has_u) perm |= 0200;
                    if (has_g) perm |= 0020;
                    if (has_o) perm |= 0002;
                    break;
                case 'x':
                    if (has_u) perm |= 0100;
                    if (has_g) perm |= 0010;
                    if (has_o) perm |= 0001;
                    break;
                case 's':
                    if (has_u) perm |= 04000; /* setuid */
                    if (has_g) perm |= 02000; /* setgid */
                    break;
                case 't':
                    perm |= 01000; /* sticky */
                    break;
                }
                p++;
            }

            if (op == '+') {
                result |= perm;
            } else if (op == '-') {
                result &= ~perm;
            } else { /* = */
                result &= ~who_bits;  /* clear who bits */
                result |= perm;
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
