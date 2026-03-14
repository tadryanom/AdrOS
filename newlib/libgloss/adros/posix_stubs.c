// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* posix_stubs.c — POSIX stubs for Bash cross-compilation on AdrOS/newlib */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <dirent.h>

static int nosys(void) { errno = ENOSYS; return -1; }

/* File descriptor ops */
int dup(int fd) { (void)fd; return nosys(); }
int dup2(int o, int n) { (void)o; (void)n; return nosys(); }
int fcntl(int fd, int c, ...) { (void)fd; (void)c; return nosys(); }
int pipe(int p[2]) { (void)p; return nosys(); }

/* File ops */
int chown(const char *p, uid_t o, gid_t g) { (void)p; (void)o; (void)g; return nosys(); }
int lstat(const char *p, struct stat *s) { return stat(p, s); }

/* Process/signal */
int killpg(int pg, int s) { return kill(-pg, s); }
int setpgid(pid_t p, pid_t g) { (void)p; (void)g; return nosys(); }
pid_t setsid(void) { return (pid_t)nosys(); }
pid_t getpgrp(void) { return getpid(); }
int sigaction(int sig, const struct sigaction *a, struct sigaction *o) {
    (void)sig; (void)a; (void)o; return nosys();
}
int sigprocmask(int h, const sigset_t *s, sigset_t *o) {
    (void)h; (void)s; (void)o; return 0;
}

/* Terminal */
int tcgetattr(int fd, struct termios *t) { (void)fd; (void)t; return nosys(); }
int tcsetattr(int fd, int a, const struct termios *t) { (void)fd; (void)a; (void)t; return nosys(); }
pid_t tcgetpgrp(int fd) { (void)fd; return (pid_t)nosys(); }
int tcsetpgrp(int fd, pid_t pg) { (void)fd; (void)pg; return nosys(); }

/* I/O multiplexing */
int select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) {
    (void)n; (void)r; (void)w; (void)e; (void)t; return nosys();
}

/* Misc */
char *getcwd(char *b, size_t sz) {
    if (b && sz > 1) { b[0] = '/'; b[1] = 0; return b; }
    errno = ERANGE; return 0;
}
int gethostname(char *n, size_t l) {
    if (n && l > 5) { strcpy(n, "adros"); return 0; }
    errno = ENAMETOOLONG; return -1;
}
int ioctl(int fd, unsigned long r, ...) { (void)fd; (void)r; return nosys(); }

/* pwd stubs */
static struct passwd _pw = {"root", "", 0, 0, "", "/", "/bin/sh"};
struct passwd *getpwuid(uid_t u) { (void)u; return &_pw; }
struct passwd *getpwnam(const char *n) { (void)n; return &_pw; }

/* Additional stubs */
int mkfifo(const char *p, mode_t m) { (void)p; (void)m; errno = ENOSYS; return -1; }
int tcdrain(int fd) { (void)fd; return 0; }
int tcflow(int fd, int a) { (void)fd; (void)a; return 0; }
int tcflush(int fd, int q) { (void)fd; (void)q; return 0; }
int tcsendbreak(int fd, int d) { (void)fd; (void)d; return 0; }
speed_t cfgetispeed(const struct termios *t) { (void)t; return 0; }
speed_t cfgetospeed(const struct termios *t) { (void)t; return 0; }
int cfsetispeed(struct termios *t, speed_t s) { (void)t; (void)s; return 0; }
int cfsetospeed(struct termios *t, speed_t s) { (void)t; (void)s; return 0; }

/* Directory stubs */
DIR *opendir(const char *p) { (void)p; errno = ENOSYS; return 0; }
struct dirent *readdir(DIR *d) { (void)d; return 0; }
int closedir(DIR *d) { (void)d; return 0; }
void rewinddir(DIR *d) { (void)d; }

/* pselect stub */
int pselect(int n, fd_set *r, fd_set *w, fd_set *e,
            const struct timespec *t, const sigset_t *sm) {
    (void)n; (void)r; (void)w; (void)e; (void)t; (void)sm;
    errno = ENOSYS; return -1;
}

/* --- Link-time stubs for Bash --- */
#include <regex.h>
#include <fnmatch.h>
#include <time.h>

int access(const char *p, int m) { (void)p; (void)m; return 0; }
unsigned alarm(unsigned s) { (void)s; return 0; }
int chdir(const char *p) { (void)p; errno = ENOSYS; return -1; }
int fchmod(int fd, mode_t m) { (void)fd; (void)m; errno = ENOSYS; return -1; }
uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getegid(void) { return 0; }
pid_t getppid(void) { return 1; }
int setuid(uid_t u) { (void)u; return 0; }
int setreuid(uid_t r, uid_t e) { (void)r; (void)e; return 0; }
int setregid(gid_t r, gid_t e) { (void)r; (void)e; return 0; }
unsigned sleep(unsigned s) { (void)s; return 0; }
mode_t umask(mode_t m) { (void)m; return 022; }
char *ttyname(int fd) { (void)fd; return "/dev/tty"; }
pid_t waitpid(pid_t p, int *s, int o) { (void)p; (void)s; (void)o; errno = ECHILD; return -1; }
void setgrent(void) {}
void endgrent(void) {}

int fnmatch(const char *pat, const char *str, int flags) {
    (void)flags;
    /* Trivial: only support exact match */
    return strcmp(pat, str) == 0 ? 0 : 1;
}

int regcomp(regex_t *r, const char *p, int f) { (void)r; (void)p; (void)f; return -1; }
int regexec(const regex_t *r, const char *s, size_t n, regmatch_t pm[], int f) {
    (void)r; (void)s; (void)n; (void)pm; (void)f; return -1;
}
void regfree(regex_t *r) { (void)r; }
size_t regerror(int e, const regex_t *r, char *buf, size_t sz) {
    (void)e; (void)r;
    if (buf && sz > 0) { buf[0] = 0; }
    return 0;
}

/* Group/user stubs */
gid_t getgid(void) { return 0; }
int setgid(gid_t g) { (void)g; return 0; }
struct group { char *gr_name; char *gr_passwd; gid_t gr_gid; char **gr_mem; };
static struct group _gr = {"root", "", 0, 0};
struct group *getgrent(void) { return &_gr; }
