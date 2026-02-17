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
#define S_ISLNK(m)  (((m) & 0170000) == 0120000)
#define S_ISBLK(m)  (((m) & 0170000) == 0060000)
#define S_ISFIFO(m) (((m) & 0170000) == 0010000)
#define S_ISSOCK(m) (((m) & 0170000) == 0140000)

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 0070
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXO 0007
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

/* stat/fstat/mkdir declared in <unistd.h> with void* for struct stat* compatibility */

#endif
