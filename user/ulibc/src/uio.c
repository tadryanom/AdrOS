#include "sys/uio.h"
#include "syscall.h"
#include "errno.h"

int readv(int fd, const struct iovec* iov, int iovcnt) {
    return __syscall_ret(_syscall3(SYS_READV, fd, (int)iov, iovcnt));
}

int writev(int fd, const struct iovec* iov, int iovcnt) {
    return __syscall_ret(_syscall3(SYS_WRITEV, fd, (int)iov, iovcnt));
}
