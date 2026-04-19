// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_UNISTD_H
#define ULIBC_UNISTD_H

#include <stdint.h>
#include <stddef.h>
#include <signal.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int     read(int fd, void* buf, size_t count);
int     write(int fd, const void* buf, size_t count);
int     open(const char* path, int flags, ...);
int     close(int fd);
int     lseek(int fd, int offset, int whence);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
int     pipe(int fds[2]);
int     fork(void);
int     execve(const char* path, char* const argv[], char* const envp[]);
int     getpid(void);
int     getppid(void);
int     chdir(const char* path);
char*   getcwd(char* buf, size_t size);
int     mkdir(const char* path, ...);  /* mode_t optional in AdrOS */
int     unlink(const char* path);
int     rmdir(const char* path);
int     setsid(void);
int     setpgid(int pid, int pgid);
int     getpgrp(void);
int     gettid(void);
int     fsync(int fd);
int     fdatasync(int fd);
int     pread(int fd, void* buf, size_t count, int offset);
int     pwrite(int fd, const void* buf, size_t count, int offset);
int     access(const char* path, int mode);
int     getuid(void);
int     getgid(void);
int     geteuid(void);
int     getegid(void);
int     setuid(int uid);
int     setgid(int gid);
int     seteuid(int euid);
int     setegid(int egid);
int     truncate(const char* path, int length);
int     ftruncate(int fd, int length);
unsigned int alarm(unsigned int seconds);
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_UN 8
#define LOCK_NB 4
int     flock(int fd, int operation);
int     umask(int mask);
int     isatty(int fd);
void*   brk(void* addr);

int     waitpid(int pid, int* status, int options);
int     getdents(int fd, void* buf, size_t count);
int     chmod(const char* path, int mode);
int     chown(const char* path, int owner, int group);
int     link(const char* oldpath, const char* newpath);
int     symlink(const char* target, const char* linkpath);
int     readlink(const char* path, char* buf, size_t bufsiz);
int     kill(int pid, int sig);
int     rename(const char* oldpath, const char* newpath);
unsigned int sleep(unsigned int seconds);
int     usleep(unsigned int usec);
int     execvp(const char* file, char* const argv[]);
int     execlp(const char* file, const char* arg, ...);
int     execl(const char* path, const char* arg, ...);
int     getopt(int argc, char* const argv[], const char* optstring);
extern char* optarg;
extern int   optind, opterr, optopt;

void    _exit(int status) __attribute__((noreturn));

/* sysconf / pathconf constants */
#define _SC_CLK_TCK        2
#define _SC_PAGE_SIZE      30
#define _SC_PAGESIZE       _SC_PAGE_SIZE
#define _SC_OPEN_MAX       4
#define _SC_NGROUPS_MAX    3
#define _SC_CHILD_MAX      1
#define _SC_ARG_MAX        0
#define _SC_HOST_NAME_MAX  180
#define _SC_LOGIN_NAME_MAX 71
#define _SC_LINE_MAX       43

#define _PC_PATH_MAX  4
#define _PC_NAME_MAX  3
#define _PC_PIPE_BUF  5
#define _PC_LINK_MAX  0

long sysconf(int name);
long pathconf(const char* path, int name);
long fpathconf(int fd, int name);

int     gethostname(char* name, size_t len);
char*   ttyname(int fd);
int     pipe2(int fds[2], int flags);
int     execle(const char* path, const char* arg, ...);
int     execveat(int dirfd, const char* path, char* const argv[], char* const envp[], int flags);
int     dup3(int oldfd, int newfd, int flags);
int     openat(int dirfd, const char* path, int flags, ...);
int     fstatat(int dirfd, const char* path, void* buf, int flags);
int     unlinkat(int dirfd, const char* path, int flags);
int     mount(const char* source, const char* target, const char* fs_type, unsigned long flags, const void* data);
int     umount2(const char* target, int flags);
int     umount(const char* target);
int     wait4(int pid, int* status, int options, void* rusage);
int     waitid(int idtype, int id, void* info, int options);
int     sigreturn(void);
int     sigqueue(int pid, int sig, const union sigval value);
int     set_thread_area(void* desc);
char*   getlogin(void);
int     getlogin_r(char* buf, size_t bufsize);
int     tcgetpgrp(int fd);
int     tcsetpgrp(int fd, int pgrp);

#define _CS_PATH 0
long    confstr(int name, char* buf, size_t len);
void*   sbrk(int increment);

/* Environment pointer (set by crt0) — POSIX standard name */
extern char** environ;

#endif
