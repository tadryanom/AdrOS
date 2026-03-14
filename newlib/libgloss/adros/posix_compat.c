// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/*
 * posix_compat.c — POSIX compatibility stubs for AdrOS cross-toolchain.
 * Provides missing functions needed by Busybox and other ported software.
 * These are thin stubs or minimal implementations.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ---- poll ---- */
#include <poll.h>
#include <sys/select.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    /* Emulate poll() via select() */
    fd_set rfds, wfds, efds;
    int maxfd = -1;
    FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        if (fds[i].fd > maxfd) maxfd = fds[i].fd;
        if (fds[i].events & POLLIN)  FD_SET(fds[i].fd, &rfds);
        if (fds[i].events & POLLOUT) FD_SET(fds[i].fd, &wfds);
        FD_SET(fds[i].fd, &efds);
    }
    struct timeval tv, *tvp = NULL;
    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }
    int ret = select(maxfd + 1, &rfds, &wfds, &efds, tvp);
    if (ret < 0) return ret;
    int count = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        if (fds[i].fd < 0) continue;
        if (FD_ISSET(fds[i].fd, &rfds))  fds[i].revents |= POLLIN;
        if (FD_ISSET(fds[i].fd, &wfds))  fds[i].revents |= POLLOUT;
        if (FD_ISSET(fds[i].fd, &efds))  fds[i].revents |= POLLERR;
        if (fds[i].revents) count++;
    }
    return count;
}

/* ---- popen / pclose ---- */
FILE *popen(const char *command, const char *type)
{
    (void)command; (void)type;
    errno = ENOSYS;
    return NULL;
}

int pclose(FILE *stream)
{
    (void)stream;
    errno = ENOSYS;
    return -1;
}

/* ---- getline / getdelim ---- */
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
    if (!lineptr || !n || !stream) { errno = EINVAL; return -1; }
    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) { errno = ENOMEM; return -1; }
    }
    size_t pos = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 2 > *n) {
            size_t newn = *n * 2;
            char *tmp = realloc(*lineptr, newn);
            if (!tmp) { errno = ENOMEM; return -1; }
            *lineptr = tmp;
            *n = newn;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == delim) break;
    }
    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    return getdelim(lineptr, n, '\n', stream);
}

/* ---- dirname / basename (libgen) ---- */
char *dirname(char *path)
{
    static char dot[] = ".";
    if (!path || !*path) return dot;
    char *last = strrchr(path, '/');
    if (!last) return dot;
    if (last == path) { path[1] = '\0'; return path; }
    *last = '\0';
    return path;
}

char *basename(char *path)
{
    static char dot[] = ".";
    if (!path || !*path) return dot;
    char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

/* ---- symlink / lchown / utimes ---- */
int symlink(const char *target, const char *linkpath)
{
    (void)target; (void)linkpath;
    errno = ENOSYS;
    return -1;
}

int lchown(const char *pathname, uid_t owner, gid_t group)
{
    (void)pathname; (void)owner; (void)group;
    errno = ENOSYS;
    return -1;
}

struct timeval;
int utimes(const char *filename, const struct timeval times[2])
{
    (void)filename; (void)times;
    return 0; /* silently succeed */
}

/* ---- getgroups ---- */
int getgroups(int size, gid_t list[])
{
    if (size == 0) return 1;
    if (size >= 1) { list[0] = getgid(); return 1; }
    errno = EINVAL;
    return -1;
}

/* ---- setpriority / getpriority ---- */
int setpriority(int which, int who, int prio)
{
    (void)which; (void)who; (void)prio;
    return 0;
}

int getpriority(int which, int who)
{
    (void)which; (void)who;
    return 0;
}

/* ---- getrlimit / setrlimit ---- */
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim)
{
    (void)resource;
    if (!rlim) { errno = EFAULT; return -1; }
    rlim->rlim_cur = RLIM_INFINITY;
    rlim->rlim_max = RLIM_INFINITY;
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
    (void)resource; (void)rlim;
    return 0;
}

/* ---- mount table stubs ---- */
#include <mntent.h>

FILE *setmntent(const char *filename, const char *type)
{
    return fopen(filename, type);
}

struct mntent *getmntent(FILE *fp)
{
    (void)fp;
    return NULL;
}

int endmntent(FILE *fp)
{
    if (fp) fclose(fp);
    return 1;
}

/* getmntent_r */
struct mntent *getmntent_r(FILE *fp, struct mntent *mntbuf,
                           char *buf, int buflen)
{
    (void)fp; (void)mntbuf; (void)buf; (void)buflen;
    return NULL;
}

/* ---- mount / umount2 ---- */
int mount(const char *source, const char *target, const char *fstype,
          unsigned long flags, const void *data)
{
    (void)source; (void)target; (void)fstype; (void)flags; (void)data;
    errno = ENOSYS;
    return -1;
}

int umount2(const char *target, int flags)
{
    (void)target; (void)flags;
    errno = ENOSYS;
    return -1;
}

/* ---- DNS stubs ---- */
#include <netdb.h>
int h_errno;

const char *hstrerror(int err)
{
    switch (err) {
    case HOST_NOT_FOUND: return "Host not found";
    case TRY_AGAIN:      return "Try again";
    case NO_RECOVERY:    return "Non-recoverable error";
    case NO_DATA:        return "No data";
    default:             return "Unknown DNS error";
    }
}

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res)
{
    (void)node; (void)service; (void)hints; (void)res;
    return -1; /* EAI_FAIL */
}

void freeaddrinfo(struct addrinfo *res)
{
    (void)res;
}

const char *gai_strerror(int errcode)
{
    (void)errcode;
    return "getaddrinfo not implemented";
}

int getnameinfo(const void *sa, unsigned int salen,
                char *host, unsigned int hostlen,
                char *serv, unsigned int servlen, int flags)
{
    (void)sa; (void)salen; (void)host; (void)hostlen;
    (void)serv; (void)servlen; (void)flags;
    return -1;
}

/* ---- mknod ---- */
int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    (void)pathname; (void)mode; (void)dev;
    errno = ENOSYS;
    return -1;
}

/* ---- execvp ---- */
int execvp(const char *file, char *const argv[])
{
    /* Simple execvp: try PATH lookup */
    if (strchr(file, '/')) return execve(file, argv, environ);
    const char *path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";
    char buf[256];
    while (*path) {
        const char *end = strchr(path, ':');
        size_t len = end ? (size_t)(end - path) : strlen(path);
        if (len + 1 + strlen(file) + 1 <= sizeof(buf)) {
            memcpy(buf, path, len);
            buf[len] = '/';
            strcpy(buf + len + 1, file);
            execve(buf, argv, environ);
            if (errno != ENOENT) return -1;
        }
        path += len;
        if (*path == ':') path++;
    }
    errno = ENOENT;
    return -1;
}

/* ---- sysinfo ---- */
#include <sys/sysinfo.h>

int sysinfo(struct sysinfo *info)
{
    if (!info) { errno = EFAULT; return -1; }
    memset(info, 0, sizeof(*info));
    info->uptime = 0;
    info->totalram = 16 * 1024 * 1024; /* 16 MB default */
    info->freeram = 8 * 1024 * 1024;
    info->mem_unit = 1;
    info->procs = 1;
    return 0;
}

/* ---- getrusage ---- */
int getrusage(int who, struct rusage *usage)
{
    (void)who;
    if (!usage) { errno = EFAULT; return -1; }
    memset(usage, 0, sizeof(*usage));
    return 0;
}

/* ---- signal ---- */
#include <signal.h>

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler)
{
    struct sigaction sa, old;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(signum, &sa, &old) < 0) return SIG_ERR;
    return old.sa_handler;
}

int sigsuspend(const sigset_t *mask)
{
    (void)mask;
    errno = EINTR;
    return -1;
}

/* ---- uname ---- */
#include <sys/utsname.h>

static int _adros_syscall1(int nr, void *arg)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg));
    return ret;
}

int uname(struct utsname *buf)
{
    if (!buf) { errno = EFAULT; return -1; }
    int r = _adros_syscall1(136, buf);
    if (r < 0) { errno = -r; return -1; }
    return 0;
}

/* ---- vfork ---- */
pid_t vfork(void)
{
    return fork(); /* emulate vfork as fork */
}

/* ---- mmap / munmap ---- */
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    if (flags & MAP_ANONYMOUS) {
        void *p = malloc(length);
        if (!p) { errno = ENOMEM; return MAP_FAILED; }
        memset(p, 0, length);
        return p;
    }
    errno = ENOSYS;
    return MAP_FAILED;
}

int munmap(void *addr, size_t length)
{
    (void)length;
    free(addr);
    return 0;
}

/* ---- rmdir ---- */
int rmdir(const char *pathname)
{
    /* Use unlink syscall with directory flag, or just unlink */
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(40), "b"(pathname));
    if (r < 0) { errno = -r; return -1; }
    return 0;
}

/* ---- ftruncate ---- */
int ftruncate(int fd, off_t length)
{
    (void)fd; (void)length;
    return 0; /* silently succeed */
}

/* ---- chroot ---- */
int chroot(const char *path)
{
    (void)path;
    errno = ENOSYS;
    return -1;
}

/* ---- readlink ---- */
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz)
{
    (void)pathname; (void)buf; (void)bufsiz;
    errno = EINVAL; /* no symlinks */
    return -1;
}

/* ---- realpath ---- */
char *realpath(const char *path, char *resolved_path)
{
    if (!path) { errno = EINVAL; return NULL; }
    if (!resolved_path) {
        resolved_path = malloc(256);
        if (!resolved_path) { errno = ENOMEM; return NULL; }
    }
    /* Simple: just copy the path */
    if (path[0] == '/') {
        strncpy(resolved_path, path, 255);
        resolved_path[255] = '\0';
    } else {
        char cwd[256];
        if (!getcwd(cwd, sizeof(cwd))) return NULL;
        snprintf(resolved_path, 256, "%s/%s", cwd, path);
    }
    return resolved_path;
}

/* ---- sysconf ---- */
long sysconf(int name)
{
    switch (name) {
    case 2:  return 200112L; /* _SC_VERSION */
    case 8:  return 4096;    /* _SC_PAGESIZE */
    case 30: return 4096;    /* _SC_PAGE_SIZE */
    case 11: return 256;     /* _SC_OPEN_MAX */
    case 84: return 256;     /* _SC_ATEXIT_MAX */
    case 4:  return 128;     /* _SC_CHILD_MAX */
    case 5:  return 100;     /* _SC_CLK_TCK */
    case 0:  return 32;      /* _SC_ARG_MAX (bytes) - actually large */
    case 3:  return 8;       /* _SC_NGROUPS_MAX */
    case 1:  return 1;       /* _SC_NPROCESSORS_CONF */
    default: errno = EINVAL; return -1;
    }
}

/* ---- fchown ---- */
int fchown(int fd, uid_t owner, gid_t group)
{
    (void)fd; (void)owner; (void)group;
    return 0;
}

/* ---- clock_settime ---- */
#include <time.h>

int clock_settime(clockid_t clk, const struct timespec *tp)
{
    (void)clk; (void)tp;
    errno = EPERM;
    return -1;
}

/* ---- utimensat ---- */
int utimensat(int dirfd, const char *pathname,
              const struct timespec times[2], int flags)
{
    (void)dirfd; (void)pathname; (void)times; (void)flags;
    return 0;
}

/* ---- clearenv ---- */
extern char **environ;

int clearenv(void)
{
    environ = NULL;
    return 0;
}

/* ---- group database stubs ---- */
#include <grp.h>

static struct group _stub_grp;
static char _stub_grp_name[] = "root";

struct group *getgrnam(const char *name)
{
    (void)name;
    _stub_grp.gr_name = _stub_grp_name;
    _stub_grp.gr_gid = 0;
    _stub_grp.gr_mem = NULL;
    return &_stub_grp;
}

struct group *getgrgid(gid_t gid)
{
    (void)gid;
    _stub_grp.gr_name = _stub_grp_name;
    _stub_grp.gr_gid = gid;
    _stub_grp.gr_mem = NULL;
    return &_stub_grp;
}

/* ---- network stubs ---- */
#include <netinet/in.h>

struct hostent *gethostbyname(const char *name)
{
    (void)name;
    h_errno = HOST_NOT_FOUND;
    return NULL;
}

int sethostname(const char *name, size_t len)
{
    (void)name; (void)len;
    return 0;
}

static char _inet_buf[16];

char *inet_ntoa(struct in_addr in)
{
    unsigned char *b = (unsigned char *)&in.s_addr;
    snprintf(_inet_buf, sizeof(_inet_buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return _inet_buf;
}

uint32_t htonl(uint32_t h) { return __builtin_bswap32(h); }
uint16_t htons(uint16_t h) { return __builtin_bswap16(h); }
uint32_t ntohl(uint32_t n) { return __builtin_bswap32(n); }
uint16_t ntohs(uint16_t n) { return __builtin_bswap16(n); }
