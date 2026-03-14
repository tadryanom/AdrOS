// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "wordexp.h"
#include "string.h"
#include "stdlib.h"

int wordexp(const char* words, wordexp_t* pwordexp, int flags) {
    if (!words || !pwordexp) return WRDE_BADCHAR;

    if (!(flags & WRDE_APPEND)) {
        pwordexp->we_wordc = 0;
        pwordexp->we_wordv = (void*)0;
    }

    /* Minimal implementation: split on whitespace, no shell expansion */
    char* copy = strdup(words);
    if (!copy) return WRDE_NOSPACE;

    size_t alloc = 8;
    size_t offs = (flags & WRDE_DOOFFS) ? pwordexp->we_offs : 0;
    char** wv = (char**)calloc(alloc + offs + 1, sizeof(char*));
    if (!wv) { free(copy); return WRDE_NOSPACE; }

    size_t count = 0;
    char* saveptr;
    char* tok = strtok_r(copy, " \t\n", &saveptr);
    while (tok) {
        if (count + offs + 1 >= alloc) {
            alloc *= 2;
            char** nv = (char**)realloc(wv, (alloc + offs + 1) * sizeof(char*));
            if (!nv) {
                for (size_t i = 0; i < count; i++) free(wv[offs + i]);
                free(wv); free(copy);
                return WRDE_NOSPACE;
            }
            wv = nv;
        }
        wv[offs + count] = strdup(tok);
        if (!wv[offs + count]) {
            for (size_t i = 0; i < count; i++) free(wv[offs + i]);
            free(wv); free(copy);
            return WRDE_NOSPACE;
        }
        count++;
        tok = strtok_r((void*)0, " \t\n", &saveptr);
    }
    wv[offs + count] = (void*)0;

    free(copy);
    pwordexp->we_wordc = count;
    pwordexp->we_wordv = wv;
    return 0;
}

void wordfree(wordexp_t* pwordexp) {
    if (!pwordexp || !pwordexp->we_wordv) return;
    for (size_t i = 0; pwordexp->we_wordv[i]; i++)
        free(pwordexp->we_wordv[i]);
    free(pwordexp->we_wordv);
    pwordexp->we_wordv = (void*)0;
    pwordexp->we_wordc = 0;
}
