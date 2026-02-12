#include "sys/ioctl.h"
#include "syscall.h"
#include "errno.h"

int ioctl(int fd, unsigned long cmd, void* arg) {
    return __syscall_ret(_syscall3(SYS_IOCTL, fd, (int)cmd, (int)arg));
}
