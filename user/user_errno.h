#ifndef USER_ERRNO_H
#define USER_ERRNO_H

extern int errno;

static inline int __syscall_fix(int ret) {
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

#endif
