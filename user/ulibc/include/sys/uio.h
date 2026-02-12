#ifndef ULIBC_SYS_UIO_H
#define ULIBC_SYS_UIO_H

#include <stddef.h>

struct iovec {
    void*   iov_base;
    size_t  iov_len;
};

int readv(int fd, const struct iovec* iov, int iovcnt);
int writev(int fd, const struct iovec* iov, int iovcnt);

#endif
