// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "string.h"
#include "stdio.h"
#include "errno.h"

static const char* _err_table[] = {
    [0]       = "Success",
    [EPERM]   = "Operation not permitted",
    [ENOENT]  = "No such file or directory",
    [ESRCH]   = "No such process",
    [EINTR]   = "Interrupted system call",
    [EIO]     = "Input/output error",
    [E2BIG]   = "Argument list too long",
    [EBADF]   = "Bad file descriptor",
    [ECHILD]  = "No child processes",
    [EAGAIN]  = "Resource temporarily unavailable",
    [ENOMEM]  = "Cannot allocate memory",
    [EACCES]  = "Permission denied",
    [EFAULT]  = "Bad address",
    [EBUSY]   = "Device or resource busy",
    [EEXIST]  = "File exists",
    [ENODEV]  = "No such device",
    [ENOTDIR] = "Not a directory",
    [EISDIR]  = "Is a directory",
    [EINVAL]  = "Invalid argument",
    [EMFILE]  = "Too many open files",
    [ENOTTY]  = "Inappropriate ioctl for device",
    [ENOSPC]  = "No space left on device",
    [ESPIPE]  = "Illegal seek",
    [EROFS]   = "Read-only file system",
    [EPIPE]   = "Broken pipe",
    [ERANGE]  = "Numerical result out of range",
    [ENAMETOOLONG] = "File name too long",
    [ENOLCK]  = "No locks available",
    [ENOSYS]  = "Function not implemented",
    [ENOTEMPTY] = "Directory not empty",
};

#define ERR_TABLE_SIZE (int)(sizeof(_err_table) / sizeof(_err_table[0]))

static char _unknown_buf[32];

char* strerror(int errnum) {
    if (errnum >= 0 && errnum < ERR_TABLE_SIZE && _err_table[errnum])
        return (char*)_err_table[errnum];
    /* Build "Unknown error NNN" */
    char* p = _unknown_buf;
    const char* prefix = "Unknown error ";
    while (*prefix) *p++ = *prefix++;
    /* itoa inline */
    int n = errnum < 0 ? -errnum : errnum;
    char tmp[12];
    int i = 0;
    if (errnum < 0) *p++ = '-';
    do { tmp[i++] = '0' + (n % 10); n /= 10; } while (n > 0);
    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    return _unknown_buf;
}

void perror(const char* s) {
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}
