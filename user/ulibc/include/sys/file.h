#ifndef ULIBC_SYS_FILE_H
#define ULIBC_SYS_FILE_H

#define LOCK_SH  1   /* shared lock */
#define LOCK_EX  2   /* exclusive lock */
#define LOCK_NB  4   /* non-blocking */
#define LOCK_UN  8   /* unlock */

int flock(int fd, int operation);

#endif
