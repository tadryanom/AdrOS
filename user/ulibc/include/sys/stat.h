#ifndef ULIBC_SYS_STAT_H
#define ULIBC_SYS_STAT_H

#include <sys/types.h>

struct stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;
    time_t    st_atime;
    time_t    st_mtime;
    time_t    st_ctime;
};

#define S_ISDIR(m)  (((m) & 0170000) == 0040000)
#define S_ISREG(m)  (((m) & 0170000) == 0100000)
#define S_ISCHR(m)  (((m) & 0170000) == 0020000)

int stat(const char* path, struct stat* buf);
int fstat(int fd, struct stat* buf);
int mkdir(const char* path, mode_t mode);

#endif
