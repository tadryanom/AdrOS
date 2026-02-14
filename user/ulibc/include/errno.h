#ifndef ULIBC_ERRNO_H
#define ULIBC_ERRNO_H

extern int errno;

#define EPERM    1
#define ENOENT   2
#define ESRCH    3
#define EINTR    4
#define EIO      5
#define ENXIO    6
#define EBADF    9
#define ECHILD  10
#define EAGAIN  11
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EEXIST  17
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define EMFILE  24
#define ENOSPC  28
#define EPIPE   32
#define ENOSYS  38
#define ENOTEMPTY 39
#define ENOLCK   37
#define EWOULDBLOCK EAGAIN

/* Convert raw syscall return to errno-style */
static inline int __syscall_ret(int r) {
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return r;
}

#endif
