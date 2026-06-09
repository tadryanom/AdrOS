// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* AdrOS ls utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

int getdents(int fd, void* buf, size_t count);

static int aflag = 0;   /* -a: show hidden files */
static int lflag = 0;   /* -l: long format */
static int nflag = 0;   /* -n: numeric UID/GID */

#define LS_MAX_ENTRIES 512

struct ls_entry {
    char name[256];
    unsigned char type;
};

static struct ls_entry entries[LS_MAX_ENTRIES];

static int cmp_entry(const void* a, const void* b) {
    return strcmp(((const struct ls_entry*)a)->name,
                  ((const struct ls_entry*)b)->name);
}

static void uid_to_name(unsigned uid, char* buf, size_t bufsz) {
    if (nflag) { snprintf(buf, bufsz, "%u", uid); return; }
    struct passwd* pw = getpwuid((int)uid);
    if (pw) { snprintf(buf, bufsz, "%s", pw->pw_name); }
    else    { snprintf(buf, bufsz, "%u", uid); }
}

static void gid_to_name(unsigned gid, char* buf, size_t bufsz) {
    if (nflag) { snprintf(buf, bufsz, "%u", gid); return; }
    struct group* gr = getgrgid((int)gid);
    if (gr) { snprintf(buf, bufsz, "%s", gr->gr_name); }
    else    { snprintf(buf, bufsz, "%u", gid); }
}

static void print_entry(const char* path, const char* display_name, unsigned char type_hint) {
    if (lflag) {
        struct stat st;
        int have_stat = (stat(path, &st) == 0);

        char type = '-';
        if (have_stat) {
            unsigned m = (unsigned)st.st_mode;
            if (S_ISDIR(m)) type = 'd';
            else if (S_ISCHR(m)) type = 'c';
            else if (S_ISLNK(m)) type = 'l';
            else if (S_ISBLK(m)) type = 'b';
        } else {
            if (type_hint == DT_DIR) type = 'd';
            else if (type_hint == DT_CHR) type = 'c';
            else if (type_hint == DT_LNK) type = 'l';
            else if (type_hint == DT_BLK) type = 'b';
        }

        char perms[10];
        if (have_stat) {
            unsigned m = (unsigned)st.st_mode;
            perms[0] = (m & S_IRUSR) ? 'r' : '-';
            perms[1] = (m & S_IWUSR) ? 'w' : '-';
            perms[2] = (m & S_IXUSR) ? 'x' : '-';
            perms[3] = (m & S_IRGRP) ? 'r' : '-';
            perms[4] = (m & S_IWGRP) ? 'w' : '-';
            perms[5] = (m & S_IXGRP) ? 'x' : '-';
            perms[6] = (m & S_IROTH) ? 'r' : '-';
            perms[7] = (m & S_IWOTH) ? 'w' : '-';
            perms[8] = (m & S_IXOTH) ? 'x' : '-';
            perms[9] = '\0';
        } else {
            strcpy(perms, "---------");
        }

        unsigned long sz = have_stat ? (unsigned long)st.st_size : 0;
        unsigned nlink = have_stat ? (unsigned)st.st_nlink : 1;

        char owner[32], group[32];
        uid_to_name(have_stat ? (unsigned)st.st_uid : 0, owner, sizeof(owner));
        gid_to_name(have_stat ? (unsigned)st.st_gid : 0, group, sizeof(group));

        char timebuf[32];
        if (have_stat) {
            time_t mtime = (time_t)st.st_mtime;
            strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&mtime));
        } else {
            strcpy(timebuf, "?");
        }

        printf("%c%s %2u %-8s %-8s %8lu %s %s\n",
               type, perms, nlink, owner, group, sz, timebuf, display_name);
    } else {
        printf("%s\n", display_name);
    }
}

static void ls_dir(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    int count = 0;
    char buf[2048];
    int rc;
    while ((rc = getdents(fd, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;

            if (!aflag && d->d_name[0] == '.') {
                off += d->d_reclen;
                continue;
            }

            if (count < LS_MAX_ENTRIES) {
                strncpy(entries[count].name, d->d_name, 255);
                entries[count].name[255] = '\0';
                entries[count].type = d->d_type;
                count++;
            }
            off += d->d_reclen;
        }
    }
    close(fd);

    qsort(entries, count, sizeof(struct ls_entry), cmp_entry);

    for (int i = 0; i < count; i++) {
        char fullpath[512];
        size_t plen = strlen(path);
        if (plen > 0 && path[plen - 1] == '/')
            snprintf(fullpath, sizeof(fullpath), "%s%s", path, entries[i].name);
        else
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entries[i].name);
        print_entry(fullpath, entries[i].name, entries[i].type);
    }
}

static void ls_path(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        ls_dir(path);
        return;
    }
    print_entry(path, path, 0);
}

int main(int argc, char** argv) {
    int npath = 0;
    const char* paths[64];

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'a') aflag = 1;
                else if (*f == 'l') lflag = 1;
                else if (*f == 'n') nflag = 1;
                else {
                    fprintf(stderr, "ls: invalid option -- '%c'\n", *f);
                    return 1;
                }
                f++;
            }
        } else {
            if (npath < 64) paths[npath++] = argv[i];
        }
    }

    if (npath == 0) {
        ls_path(".");
    } else {
        for (int i = 0; i < npath; i++) {
            if (npath > 1) printf("%s:\n", paths[i]);
            ls_path(paths[i]);
            if (npath > 1 && i < npath - 1) printf("\n");
        }
    }

    return 0;
}
