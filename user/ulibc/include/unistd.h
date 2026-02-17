#ifndef ULIBC_UNISTD_H
#define ULIBC_UNISTD_H

#include <stdint.h>
#include <stddef.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int     read(int fd, void* buf, size_t count);
int     write(int fd, const void* buf, size_t count);
int     open(const char* path, int flags);
int     close(int fd);
int     lseek(int fd, int offset, int whence);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
int     pipe(int fds[2]);
int     fork(void);
int     execve(const char* path, const char* const* argv, const char* const* envp);
int     getpid(void);
int     getppid(void);
int     chdir(const char* path);
int     getcwd(char* buf, size_t size);
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
int     isatty(int fd);
void*   brk(void* addr);

int     waitpid(int pid, int* status, int options);
int     getdents(int fd, void* buf, size_t count);
int     stat(const char* path, void* buf);  /* use sys/stat.h for typed version */
int     fstat(int fd, void* buf);              /* use sys/stat.h for typed version */
int     chmod(const char* path, int mode);
int     chown(const char* path, int owner, int group);
int     link(const char* oldpath, const char* newpath);
int     symlink(const char* target, const char* linkpath);
int     readlink(const char* path, char* buf, size_t bufsiz);
int     kill(int pid, int sig);
int     rename(const char* oldpath, const char* newpath);

void    _exit(int status) __attribute__((noreturn));

/* Environment pointer (set by crt0) */
extern char** __environ;

#endif
