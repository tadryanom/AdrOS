// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS grep utility — search for pattern in files */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#include <ctype.h>

static int use_regex = 1;  /* 1=BRE, 2=ERE, 0=fixed string */
static int icase = 0;
static int show_name = 0;
static int invert = 0;
static int count_only = 0;
static int line_num = 0;
static int list_files = 0;   /* -l: print files with match */
static int quiet = 0;        /* -q: no output, just exit code */
static int recursive = 0;    /* -r: recurse directories */

static regex_t compiled_re;
static int re_compiled = 0;
static char fixed_pattern[256];

static int match_line(const char* text) {
    if (use_regex == 0) {
        /* Fixed string match */
        if (icase) {
            const char* p = text;
            size_t plen = strlen(fixed_pattern);
            while (*p) {
                int i;
                for (i = 0; i < (int)plen && p[i]; i++) {
                    char a = (char)toupper((unsigned char)p[i]);
                    char b = (char)toupper((unsigned char)fixed_pattern[i]);
                    if (a != b) break;
                }
                if (i == (int)plen) return 1;
                p++;
            }
            return 0;
        }
        return strstr(text, fixed_pattern) != NULL;
    }
    /* Regex match */
    int eflags = 0;
    int rc = regexec(&compiled_re, text, 0, NULL, eflags);
    return (rc == 0);
}

static int grep_fd(int fd, const char* fname) {
    char buf[4096];
    int pos = 0, n, matches = 0, lnum = 0;
    while ((n = read(fd, buf + pos, (size_t)(sizeof(buf) - 1 - pos))) > 0) {
        pos += n;
        buf[pos] = '\0';
        char* start = buf;
        char* nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            lnum++;
            int m = match_line(start);
            if (invert) m = !m;
            if (m) {
                matches++;
                if (list_files) return 1;
                if (!count_only && !quiet) {
                    if (show_name) printf("%s:", fname);
                    if (line_num) printf("%d:", lnum);
                    printf("%s\n", start);
                }
            }
            start = nl + 1;
        }
        int rem = (int)(buf + pos - start);
        if (rem > 0) memmove(buf, start, (size_t)rem);
        pos = rem;
    }
    if (pos > 0) {
        buf[pos] = '\0';
        lnum++;
        int m = match_line(buf);
        if (invert) m = !m;
        if (m) {
            matches++;
            if (list_files) return 1;
            if (!count_only && !quiet) {
                if (show_name) printf("%s:", fname);
                if (line_num) printf("%d:", lnum);
                printf("%s\n", buf);
            }
        }
    }
    if (list_files && matches > 0) {
        printf("%s\n", fname);
        return 0;
    }
    if (count_only && !quiet)
        printf("%s%s%d\n", show_name ? fname : "", show_name ? ":" : "", matches);
    return matches > 0 ? 0 : 1;
}

static void grep_recursive(const char* path, const char* fname);

static void grep_dir(const char* path, const char* prefix) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    char dbuf[2048];
    int rc;
    while ((rc = getdents(fd, dbuf, sizeof(dbuf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(dbuf + off);
            if (d->d_reclen == 0) break;
            if (d->d_name[0] == '.' && (d->d_name[1] == '\0' ||
                (d->d_name[1] == '.' && d->d_name[2] == '\0'))) {
                off += d->d_reclen;
                continue;
            }
            char child[512];
            size_t plen = strlen(path);
            if (plen > 0 && path[plen - 1] == '/')
                snprintf(child, sizeof(child), "%s%s", path, d->d_name);
            else
                snprintf(child, sizeof(child), "%s/%s", path, d->d_name);
            grep_recursive(child, prefix);
            off += d->d_reclen;
        }
    }
    close(fd);
}

static void grep_recursive(const char* path, const char* prefix) {
    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "grep: %s: No such file or directory\n", path);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        grep_dir(path, prefix);
    } else {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "grep: %s: No such file or directory\n", path);
            return;
        }
        grep_fd(fd, path);
        close(fd);
    }
}

int main(int argc, char** argv) {
    int i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'v') invert = 1;
            else if (argv[i][j] == 'c') count_only = 1;
            else if (argv[i][j] == 'n') line_num = 1;
            else if (argv[i][j] == 'i') icase = 1;
            else if (argv[i][j] == 'l') list_files = 1;
            else if (argv[i][j] == 'q') quiet = 1;
            else if (argv[i][j] == 'r') recursive = 1;
            else if (argv[i][j] == 'E') use_regex = 2;
            else if (argv[i][j] == 'F') use_regex = 0;
            else if (argv[i][j] == 'e' && j == 1) {
                /* -e PATTERN */
                if (argv[i][j+1]) { /* pattern follows in same arg */ }
                /* handled below */
            }
            else {
                fprintf(stderr, "grep: invalid option -- '%c'\n", argv[i][j]);
                return 2;
            }
        }
        i++;
    }
    if (i >= argc) { fprintf(stderr, "usage: grep [-vcnlqiErF] PATTERN [FILE...]\n"); return 2; }
    const char* pattern = argv[i++];

    /* Compile regex or set fixed pattern */
    if (use_regex == 0) {
        strncpy(fixed_pattern, pattern, sizeof(fixed_pattern) - 1);
        fixed_pattern[sizeof(fixed_pattern) - 1] = '\0';
    } else {
        int cflags = (use_regex == 2) ? REG_EXTENDED : 0;
        if (icase) cflags |= REG_ICASE;
        if (regcomp(&compiled_re, pattern, cflags) != 0) {
            fprintf(stderr, "grep: invalid regex: %s\n", pattern);
            return 2;
        }
        re_compiled = 1;
    }

    if (i >= argc) {
        int rc = grep_fd(STDIN_FILENO, "(stdin)");
        if (re_compiled) regfree(&compiled_re);
        return rc;
    }

    int nfiles = argc - i;
    if (nfiles > 1 || recursive) show_name = 1;
    int rc = 1;
    for (; i < argc; i++) {
        if (recursive) {
            grep_recursive(argv[i], argv[i]);
            rc = 0;  /* simplified */
        } else {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) { fprintf(stderr, "grep: %s: No such file or directory\n", argv[i]); continue; }
            if (grep_fd(fd, argv[i]) == 0) rc = 0;
            close(fd);
        }
    }
    if (re_compiled) regfree(&compiled_re);
    return rc;
}
