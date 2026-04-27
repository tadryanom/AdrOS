// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS ps utility — process status */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>

static int eflag = 0;  /* -e: all processes (same as -A) */
static int fflag = 0;  /* -f: full format */
static int aflag = 0;  /* a: BSD all with tty */
static int uflag = 0;  /* u: BSD user-oriented */
static int xflag = 0;  /* x: BSD include no-tty */

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "-A") == 0) eflag = 1;
        else if (strcmp(argv[i], "-f") == 0) fflag = 1;
        else if (strcmp(argv[i], "-ef") == 0 || strcmp(argv[i], "-fe") == 0) { eflag = 1; fflag = 1; }
        else if (strcmp(argv[i], "aux") == 0 || strcmp(argv[i], "-aux") == 0) { aflag = 1; uflag = 1; xflag = 1; }
        else if (strcmp(argv[i], "ax") == 0 || strcmp(argv[i], "-ax") == 0) { aflag = 1; xflag = 1; }
        else if (strcmp(argv[i], "-u") == 0) uflag = 1;
        else if (strcmp(argv[i], "--") == 0) break;
    }

    /* Read /proc */
    int fd = open("/proc", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ps: cannot open /proc\n");
        return 1;
    }

    /* Print header */
    if (fflag) {
        printf("%-8s %5s %5s %5s %-8s %-8s %5s %-6s %s\n",
               "UID", "PID", "PPID", "C", "STIME", "TTY", "TIME", "STAT", "CMD");
    } else if (uflag) {
        printf("%-8s %5s %5s %5s %5s %-8s %5s %-6s %s\n",
               "USER", "PID", "%CPU", "%MEM", "VSZ", "RSS", "TTY", "STAT", "COMMAND");
    } else {
        printf("%5s %5s %-6s %-8s %s\n",
               "PID", "PPID", "STAT", "TTY", "CMD");
    }

    char dbuf[2048];
    int rc;
    while ((rc = getdents(fd, dbuf, sizeof(dbuf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(dbuf + off);
            if (d->d_reclen == 0) break;

            /* Only process numeric directories */
            int is_pid = 1;
            for (int i = 0; d->d_name[i]; i++) {
                if (d->d_name[i] < '0' || d->d_name[i] > '9') { is_pid = 0; break; }
            }
            if (is_pid && d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                int pid = atoi(d->d_name);

                /* Read /proc/<pid>/status */
                char path[64];
                snprintf(path, sizeof(path), "/proc/%d/status", pid);
                int sfd = open(path, O_RDONLY);
                if (sfd >= 0) {
                    char sbuf[512];
                    int sn = read(sfd, sbuf, sizeof(sbuf) - 1);
                    close(sfd);
                    if (sn > 0) {
                        sbuf[sn] = '\0';
                        /* Parse: PID PPID STATE CMD ... */
                        int ppid = 0, uid = 0;
                        char state = '?';
                        char cmd[128] = "";
                        char tty[8] = "?";

                        /* Simple parse: fields separated by spaces */
                        char* sp = sbuf;
                        int field = 0;
                        while (*sp) {
                            while (*sp == ' ') sp++;
                            if (!*sp) break;
                            char* start = sp;
                            while (*sp && *sp != ' ' && *sp != '\n') sp++;
                            char saved = *sp;
                            *sp = '\0';
                            switch (field) {
                            case 0: /* PID — already have it */ break;
                            case 1: ppid = atoi(start); break;
                            case 2: state = start[0]; break;
                            case 3: strncpy(cmd, start, sizeof(cmd) - 1); break;
                            case 4: uid = atoi(start); break;
                            case 5: strncpy(tty, start, sizeof(tty) - 1); break;
                            }
                            *sp = saved;
                            field++;
                            if (saved == '\n') break;
                        }

                        /* Resolve uid */
                        char uname[32];
                        struct passwd* pw = getpwuid(uid);
                        if (pw) strncpy(uname, pw->pw_name, sizeof(uname) - 1);
                        else snprintf(uname, sizeof(uname), "%d", uid);

                        if (fflag) {
                            printf("%-8s %5d %5d %5d %-8s %-8s %5s %-6c %s\n",
                                   uname, pid, ppid, 0, "00:00", tty, "00:00", state, cmd);
                        } else if (uflag) {
                            printf("%-8s %5d %5s %5s %5s %-8s %5s %-6c %s\n",
                                   uname, pid, "0.0", "0.0", "0", "0", tty, state, cmd);
                        } else {
                            printf("%5d %5d %-6c %-8s %s\n",
                                   pid, ppid, state, tty, cmd);
                        }
                    }
                }
            }
            off += d->d_reclen;
        }
    }
    close(fd);
    return 0;
}
