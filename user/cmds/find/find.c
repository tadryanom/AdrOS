// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS find utility — search for files in directory hierarchy */
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif

static const char* name_pattern = NULL;
static int type_filter = 0; /* 0=any, 'f'=file, 'd'=dir */

static int match_name(const char* name) {
    if (!name_pattern) return 1;
    /* Simple wildcard: *pattern* if pattern has no special chars,
       or exact match. Support leading/trailing * only. */
    int plen = (int)strlen(name_pattern);
    if (plen == 0) return 1;

    const char* pat = name_pattern;
    int lead_star = (pat[0] == '*');
    int trail_star = (plen > 1 && pat[plen-1] == '*');

    if (lead_star && trail_star) {
        char sub[256];
        int slen = plen - 2;
        if (slen <= 0) return 1;
        memcpy(sub, pat + 1, (size_t)slen);
        sub[slen] = '\0';
        return strstr(name, sub) != NULL;
    }
    if (lead_star) {
        const char* suffix = pat + 1;
        int slen = plen - 1;
        int nlen = (int)strlen(name);
        if (nlen < slen) return 0;
        return strcmp(name + nlen - slen, suffix) == 0;
    }
    if (trail_star) {
        return strncmp(name, pat, (size_t)(plen - 1)) == 0;
    }
    return strcmp(name, pat) == 0;
}

static void find_recurse(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* d;
    while ((d = readdir(dir)) != NULL) {
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        char child[512];
        size_t plen = strlen(path);
        if (plen > 0 && path[plen - 1] == '/')
            snprintf(child, sizeof(child), "%s%s", path, d->d_name);
        else
            snprintf(child, sizeof(child), "%s/%s", path, d->d_name);

        int is_dir = (d->d_type == DT_DIR);
        int show = match_name(d->d_name);
        if (show && type_filter) {
            if (type_filter == 'f' && is_dir) show = 0;
            if (type_filter == 'd' && !is_dir) show = 0;
        }
        if (show) printf("%s\n", child);

        if (is_dir) {
            find_recurse(child);
        }
    }
    closedir(dir);
}

int main(int argc, char** argv) {
    const char* start = ".";
    int argi = 1;

    if (argi < argc && argv[argi][0] != '-') {
        start = argv[argi++];
    }

    while (argi < argc) {
        if (strcmp(argv[argi], "-name") == 0 && argi + 1 < argc) {
            name_pattern = argv[++argi];
        } else if (strcmp(argv[argi], "-type") == 0 && argi + 1 < argc) {
            type_filter = argv[++argi][0];
        }
        argi++;
    }

    printf("%s\n", start);
    find_recurse(start);
    return 0;
}
