// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS kill utility — send signal to process */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: kill [-SIGNAL] PID...\n");
        return 1;
    }

    int sig = 15; /* SIGTERM */
    int start = 1;

    if (argv[1][0] == '-') {
        const char* s = argv[1] + 1;
        if (strcmp(s, "l") == 0) {
            /* List signal names */
            printf(" 1) SIGHUP   2) SIGINT   3) SIGQUIT  4) SIGILL   5) SIGTRAP\n");
            printf(" 6) SIGABRT  7) SIGBUS   8) SIGFPE   9) SIGKILL 10) SIGUSR1\n");
            printf("11) SIGSEGV 12) SIGUSR2 13) SIGPIPE 14) SIGALRM 15) SIGTERM\n");
            printf("16) SIGSTKFLT 17) SIGCHLD 18) SIGCONT 19) SIGSTOP 20) SIGTSTP\n");
            printf("21) SIGTTIN 22) SIGTTOU 23) SIGURG  24) SIGXCPU 25) SIGXFSZ\n");
            printf("26) SIGVTALRM 27) SIGPROF 28) SIGWINCH 29) SIGIO 30) SIGPWR\n");
            return 0;
        }
        if (strcmp(s, "9") == 0 || strcmp(s, "KILL") == 0) sig = 9;
        else if (strcmp(s, "15") == 0 || strcmp(s, "TERM") == 0) sig = 15;
        else if (strcmp(s, "2") == 0 || strcmp(s, "INT") == 0) sig = 2;
        else if (strcmp(s, "1") == 0 || strcmp(s, "HUP") == 0) sig = 1;
        else if (strcmp(s, "0") == 0) sig = 0;
        else sig = atoi(s);
        start = 2;
    }

    int rc = 0;
    for (int i = start; i < argc; i++) {
        int pid = atoi(argv[i]);
        if (pid <= 0) {
            fprintf(stderr, "kill: invalid pid '%s'\n", argv[i]);
            rc = 1;
            continue;
        }
        if (kill(pid, sig) < 0) {
            fprintf(stderr, "kill: %d: no such process\n", pid);
            rc = 1;
        }
    }
    return rc;
}
