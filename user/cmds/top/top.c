// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS top utility — one-shot process listing with system summary */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

static int is_digit(char c) { return c >= '0' && c <= '9'; }

int main(int argc, char** argv) {
    int n_iter = 1;  /* default: one-shot */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) { n_iter = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--") == 0) break;
    }

    for (int iter = 0; iter < n_iter; iter++) {
        if (iter > 0) sleep(2);

        /* Print system summary header */
        time_t now = time((time_t*)0);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", localtime(&now));

        /* Count processes */
        int nproc = 0, nrunning = 0, nsleeping = 0;
        int pfd = open("/proc", O_RDONLY);
        if (pfd >= 0) {
            char dbuf[2048];
            int rc;
            while ((rc = getdents(pfd, dbuf, sizeof(dbuf))) > 0) {
                int off = 0;
                while (off < rc) {
                    struct dirent* d = (struct dirent*)(dbuf + off);
                    if (d->d_reclen == 0) break;
                    if (is_digit(d->d_name[0])) {
                        nproc++;
                        char spath[64];
                        snprintf(spath, sizeof(spath), "/proc/%s/status", d->d_name);
                        int sfd = open(spath, O_RDONLY);
                        if (sfd >= 0) {
                            char sbuf[256];
                            int sn = read(sfd, sbuf, sizeof(sbuf) - 1);
                            close(sfd);
                            if (sn > 0) {
                                sbuf[sn] = '\0';
                                /* Parse state field: "PID PPID STATE ..." */
                                char* sp = sbuf;
                                int field = 0;
                                while (*sp) {
                                    while (*sp == ' ') sp++;
                                    if (!*sp) break;
                                    char* start = sp;
                                    while (*sp && *sp != ' ' && *sp != '\n') sp++;
                                    if (field == 2) {
                                        char st = start[0];
                                        if (st == 'R') nrunning++;
                                        else nsleeping++;
                                    }
                                    field++;
                                    if (*sp == '\n') break;
                                }
                            }
                        }
                    }
                    off += d->d_reclen;
                }
            }
            close(pfd);
        }

        printf("top - %s up ?,  %d users,  load average: 0.00\n", timebuf, 1);
        printf("Tasks: %d total, %d running, %d sleeping\n", nproc, nrunning, nsleeping);
        printf("%%Cpu(s): 0.0 us, 0.0 sy, 0.0 ni, 100.0 id\n");
        printf("MiB Mem:  0.0 total, 0.0 free, 0.0 used\n");
        printf("\n");
        printf("%-8s %5s %5s %-6s %-8s %5s %s\n",
               "USER", "PID", "PPID", "STAT", "TTY", "TIME", "CMD");

        /* List processes */
        pfd = open("/proc", O_RDONLY);
        if (pfd < 0) {
            fprintf(stderr, "top: cannot open /proc\n");
            return 1;
        }
        char buf[2048];
        int rc;
        while ((rc = getdents(pfd, buf, sizeof(buf))) > 0) {
            int off = 0;
            while (off < rc) {
                struct dirent* d = (struct dirent*)(buf + off);
                if (d->d_reclen == 0) break;
                if (is_digit(d->d_name[0])) {
                    char path[64];
                    int pid = atoi(d->d_name);

                    /* Read status */
                    snprintf(path, sizeof(path), "/proc/%s/status", d->d_name);
                    int sfd = open(path, O_RDONLY);
                    if (sfd >= 0) {
                        char sbuf[512];
                        int sn = read(sfd, sbuf, sizeof(sbuf) - 1);
                        close(sfd);
                        if (sn > 0) {
                            sbuf[sn] = '\0';
                            int ppid = 0, uid = 0;
                            char state = '?';
                            char cmd[128] = "";
                            char tty[8] = "?";
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
                            char uname[16] = "root";
                            if (uid != 0) snprintf(uname, sizeof(uname), "%d", uid);
                            printf("%-8s %5d %5d %-6c %-8s %5s %s\n",
                                   uname, pid, ppid, state, tty, "0:00", cmd);
                        }
                    }
                }
                off += d->d_reclen;
            }
        }
        close(pfd);

        if (iter < n_iter - 1) printf("\n");
    }
    return 0;
}
