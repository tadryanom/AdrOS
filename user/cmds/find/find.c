// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS find utility — search for files in a directory hierarchy */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <stdlib.h>

#define MAX_PRED 32

enum pred_type {
    PRED_NAME, PRED_TYPE, PRED_MTIME, PRED_SIZE, PRED_PERM,
    PRED_USER, PRED_MAXDEPTH, PRED_DELETE, PRED_EXEC, PRED_PRINT
};

struct predicate {
    enum pred_type type;
    char arg[256];
    int negate;
    /* -mtime: n means exactly n days, +n means > n, -n means < n */
    int cmp_sign;  /* 0=exact, 1=greater, -1=less */
    int n_arg;
    /* -perm */
    unsigned int perm_val;
    /* -exec args */
    char exec_cmd[256];
    char exec_args[8][64];
    int exec_nargs;
};

static struct predicate preds[MAX_PRED];
static int npreds = 0;
static int maxdepth = -1;
static int did_print = 0;  /* whether -print was explicitly given */

static int match_name(const char* name, const char* pattern) {
    /* Simple glob: * matches anything, ? matches one char */
    const char* s = name, *p = pattern;
    while (*s && *p) {
        if (*p == '*') {
            p++;
            if (!*p) return 1;
            while (*s) {
                if (match_name(s, p)) return 1;
                s++;
            }
            return match_name(s, p);
        }
        if (*p == '?' || *p == *s) { p++; s++; continue; }
        return 0;
    }
    while (*p == '*') p++;
    return (*s == '\0' && *p == '\0');
}

static int eval_pred(struct predicate* pr, const char* path, const struct stat* st, int depth) {
    int result = 1;
    switch (pr->type) {
    case PRED_NAME: {
        const char* name = strrchr(path, '/');
        name = name ? name + 1 : path;
        result = match_name(name, pr->arg);
        break;
    }
    case PRED_TYPE:
        if (pr->arg[0] == 'f') result = S_ISREG(st->st_mode);
        else if (pr->arg[0] == 'd') result = S_ISDIR(st->st_mode);
        else if (pr->arg[0] == 'c') result = S_ISCHR(st->st_mode);
        else if (pr->arg[0] == 'b') result = S_ISBLK(st->st_mode);
        else if (pr->arg[0] == 'l') result = S_ISLNK(st->st_mode);
        else if (pr->arg[0] == 'p') result = S_ISFIFO(st->st_mode);
        else result = 0;
        break;
    case PRED_MTIME: {
        time_t now = time((time_t*)0);
        int days = (int)((now - (time_t)st->st_mtime) / 86400);
        if (pr->cmp_sign == 0) result = (days == pr->n_arg);
        else if (pr->cmp_sign == 1) result = (days > pr->n_arg);
        else result = (days < pr->n_arg);
        break;
    }
    case PRED_SIZE: {
        long sz = (long)st->st_size;
        long n = (long)pr->n_arg;
        if (pr->cmp_sign == 0) result = (sz == n);
        else if (pr->cmp_sign == 1) result = (sz > n);
        else result = (sz < n);
        break;
    }
    case PRED_PERM:
        result = ((unsigned int)(st->st_mode & 07777) == pr->perm_val);
        break;
    case PRED_USER: {
        struct passwd* pw = getpwnam(pr->arg);
        if (pw) result = ((int)st->st_uid == pw->pw_uid);
        else { int uid = atoi(pr->arg); result = ((int)st->st_uid == uid); }
        break;
    }
    case PRED_MAXDEPTH:
        result = (depth <= pr->n_arg);
        break;
    case PRED_DELETE:
        if (S_ISDIR(st->st_mode)) result = (rmdir(path) == 0);
        else result = (unlink(path) == 0);
        if (result) return 1;  /* deleted, don't print */
        break;
    case PRED_EXEC: {
        /* Replace {} with path */
        char* argv[10];
        argv[0] = pr->exec_cmd;
        for (int i = 0; i < pr->exec_nargs; i++) {
            if (strcmp(pr->exec_args[i], "{}") == 0)
                argv[i + 1] = (char*)path;
            else
                argv[i + 1] = pr->exec_args[i];
        }
        argv[pr->exec_nargs + 1] = (char*)0;
        int pid = fork();
        if (pid == 0) {
            extern char** environ;
            execve(pr->exec_cmd, argv, environ);
            _exit(127);
        }
        int status = 0;
        if (pid > 0) waitpid(pid, &status, 0);
        result = (pid > 0 && status == 0);
        break;
    }
    case PRED_PRINT:
        result = 1;
        break;
    }
    if (pr->negate) result = !result;
    return result;
}

static void find_recurse(const char* path, int depth);

static void find_dir(const char* path, int depth) {
    if (maxdepth >= 0 && depth > maxdepth) return;
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
            find_recurse(child, depth + 1);
            off += d->d_reclen;
        }
    }
    close(fd);
}

static void find_recurse(const char* path, int depth) {
    struct stat st;
    if (stat(path, &st) < 0) return;

    int match = 1;
    int do_print = !did_print;
    for (int i = 0; i < npreds && match; i++) {
        if (preds[i].type == PRED_PRINT) { do_print = 1; continue; }
        if (preds[i].type == PRED_MAXDEPTH) {
            if (depth > preds[i].n_arg) { match = 0; break; }
            continue;
        }
        match = eval_pred(&preds[i], path, &st, depth);
    }

    if (match && do_print) printf("%s\n", path);

    if (match && S_ISDIR(st.st_mode)) {
        /* Check if any predicate was -delete and matched */
        int deleted = 0;
        for (int i = 0; i < npreds; i++) {
            if (preds[i].type == PRED_DELETE && eval_pred(&preds[i], path, (const struct stat*)&st, depth)) {
                deleted = 1; break;
            }
        }
        if (!deleted) find_dir(path, depth);
    }
}

int main(int argc, char** argv) {
    const char* start = ".";
    int argi = 1;

    /* First non-option arg is starting path */
    if (argi < argc && argv[argi][0] != '-' && strcmp(argv[argi], "--") != 0) {
        start = argv[argi++];
    }

    while (argi < argc) {
        if (strcmp(argv[argi], "--") == 0) { argi++; break; }
        if (strcmp(argv[argi], "-name") == 0 && argi + 1 < argc) {
            preds[npreds].type = PRED_NAME;
            strncpy(preds[npreds].arg, argv[++argi], sizeof(preds[npreds].arg) - 1);
            npreds++;
        } else if (strcmp(argv[argi], "-type") == 0 && argi + 1 < argc) {
            preds[npreds].type = PRED_TYPE;
            strncpy(preds[npreds].arg, argv[++argi], sizeof(preds[npreds].arg) - 1);
            npreds++;
        } else if (strcmp(argv[argi], "-mtime") == 0 && argi + 1 < argc) {
            argi++;
            preds[npreds].type = PRED_MTIME;
            const char* a = argv[argi];
            if (a[0] == '+') { preds[npreds].cmp_sign = 1; preds[npreds].n_arg = atoi(a + 1); }
            else if (a[0] == '-') { preds[npreds].cmp_sign = -1; preds[npreds].n_arg = atoi(a + 1); }
            else { preds[npreds].cmp_sign = 0; preds[npreds].n_arg = atoi(a); }
            npreds++;
        } else if (strcmp(argv[argi], "-size") == 0 && argi + 1 < argc) {
            argi++;
            preds[npreds].type = PRED_SIZE;
            const char* a = argv[argi];
            int mul = 1;
            int alen = (int)strlen(a);
            if (alen > 0 && (a[alen-1] == 'c' || a[alen-1] == 'C')) mul = 1;
            else if (alen > 0 && a[alen-1] == 'k') mul = 1024;
            else if (alen > 0 && a[alen-1] == 'M') mul = 1024*1024;
            else mul = 512; /* default: 512-byte blocks */
            if (a[0] == '+') { preds[npreds].cmp_sign = 1; preds[npreds].n_arg = atoi(a + 1) * mul; }
            else if (a[0] == '-') { preds[npreds].cmp_sign = -1; preds[npreds].n_arg = atoi(a + 1) * mul; }
            else { preds[npreds].cmp_sign = 0; preds[npreds].n_arg = atoi(a) * mul; }
            npreds++;
        } else if (strcmp(argv[argi], "-perm") == 0 && argi + 1 < argc) {
            preds[npreds].type = PRED_PERM;
            preds[npreds].perm_val = (unsigned int)strtol(argv[++argi], NULL, 8);
            npreds++;
        } else if (strcmp(argv[argi], "-user") == 0 && argi + 1 < argc) {
            preds[npreds].type = PRED_USER;
            strncpy(preds[npreds].arg, argv[++argi], sizeof(preds[npreds].arg) - 1);
            npreds++;
        } else if (strcmp(argv[argi], "-maxdepth") == 0 && argi + 1 < argc) {
            preds[npreds].type = PRED_MAXDEPTH;
            preds[npreds].n_arg = atoi(argv[++argi]);
            maxdepth = preds[npreds].n_arg;
            npreds++;
        } else if (strcmp(argv[argi], "-delete") == 0) {
            preds[npreds].type = PRED_DELETE;
            npreds++;
        } else if (strcmp(argv[argi], "-print") == 0) {
            preds[npreds].type = PRED_PRINT;
            did_print = 1;
            npreds++;
        } else if (strcmp(argv[argi], "-exec") == 0 && argi + 1 < argc) {
            preds[npreds].type = PRED_EXEC;
            strncpy(preds[npreds].exec_cmd, argv[++argi], sizeof(preds[npreds].exec_cmd) - 1);
            preds[npreds].exec_nargs = 0;
            while (argi + 1 < argc && strcmp(argv[argi + 1], ";") != 0 &&
                   preds[npreds].exec_nargs < 7) {
                argi++;
                strncpy(preds[npreds].exec_args[preds[npreds].exec_nargs],
                        argv[argi], sizeof(preds[npreds].exec_args[0]) - 1);
                preds[npreds].exec_nargs++;
            }
            if (argi + 1 < argc && strcmp(argv[argi + 1], ";") == 0) argi++;
            npreds++;
        } else if (strcmp(argv[argi], "!") == 0 && npreds > 0) {
            preds[npreds - 1].negate = 1;
        } else {
            fprintf(stderr, "find: invalid predicate '%s'\n", argv[argi]);
            return 1;
        }
        argi++;
    }

    find_recurse(start, 0);
    return 0;
}
