// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <termios.h>
#include <sys/types.h>
#include <utmp.h>
#include <sys/utsname.h>

/* Simple login command */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("AdrOS Login\n");

    while (1) {
        char username[64];
        char password[64];
        char tty_name[32] = "ttyS0";  /* Default TTY */

        /* Get TTY name from environment or default */
        char* tty = ttyname(STDIN_FILENO);
        if (tty) {
            /* Strip /dev/ prefix if present */
            if (strncmp(tty, "/dev/", 5) == 0) {
                strncpy(tty_name, tty + 5, sizeof(tty_name) - 1);
            } else {
                strncpy(tty_name, tty, sizeof(tty_name) - 1);
            }
            tty_name[sizeof(tty_name) - 1] = '\0';
        }

        /* Prompt for username */
        printf("login: ");
        fflush(stdout);

        if (fgets(username, sizeof(username), stdin) == NULL) {
            break;
        }

        /* Remove trailing newline */
        size_t len = strlen(username);
        if (len > 0 && username[len - 1] == '\n') {
            username[len - 1] = '\0';
        }

        if (username[0] == '\0') {
            continue;
        }

        /* Prompt for password (no echo) */
        printf("Password: ");
        fflush(stdout);

        /* Disable echo */
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        if (fgets(password, sizeof(password), stdin) == NULL) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            printf("\n");
            break;
        }

        /* Restore echo */
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        printf("\n");

        /* Remove trailing newline */
        len = strlen(password);
        if (len > 0 && password[len - 1] == '\n') {
            password[len - 1] = '\0';
        }

        /* Verify password */
        if (check_password(username, password) == 0) {
            /* Authentication successful */
            struct passwd* pw = getpwnam(username);
            if (pw) {
                printf("Welcome %s!\n", pw->pw_name);

                /* Set UID/GID */
                if (setgid(pw->pw_gid) < 0) {
                    perror("setgid");
                    continue;
                }
                if (setuid(pw->pw_uid) < 0) {
                    perror("setuid");
                    continue;
                }

                /* Change to home directory */
                if (pw->pw_dir && pw->pw_dir[0] != '\0') {
                    chdir(pw->pw_dir);
                }

                /* Register login in utmp */
                struct utsname uts;
                char hostname[64] = "localhost";
                if (uname(&uts) == 0) {
                    strncpy(hostname, uts.nodename, sizeof(hostname) - 1);
                    hostname[sizeof(hostname) - 1] = '\0';
                }
                utmp_login(getpid(), tty_name, username, hostname);

                /* Execute shell */
                char* shell = pw->pw_shell;
                if (!shell || shell[0] == '\0') {
                    shell = "/bin/sh";
                }

                execl(shell, shell, NULL);
                perror("execl");
                exit(1);
            } else {
                printf("Login failed (user not found)\n");
            }
        } else {
            printf("Login incorrect\n");
        }
    }

    return 1;
}
