// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS which utility — locate a command */
#include <stdio.h>
#include <string.h>
#include <dirent.h>

static int exists_in_dir(const char* dirname, const char* name) {
    DIR* dir = opendir(dirname);
    if (!dir) return 0;
    struct dirent* d;
    while ((d = readdir(dir)) != NULL) {
        if (strcmp(d->d_name, name) == 0) {
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: which command\n");
        return 1;
    }

    static const char* path_dirs[] = { "/bin", "/sbin", NULL };
    int ret = 1;

    for (int i = 1; i < argc; i++) {
        int found = 0;
        for (int d = 0; path_dirs[d]; d++) {
            if (exists_in_dir(path_dirs[d], argv[i])) {
                printf("%s/%s\n", path_dirs[d], argv[i]);
                found = 1;
                break;
            }
        }
        if (found) ret = 0;
    }
    return ret;
}
