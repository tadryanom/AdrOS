// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_TYPES_H
#define ULIBC_SYS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t  ssize_t;
typedef uint32_t off_t;
typedef uint32_t mode_t;
typedef uint32_t pid_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t dev_t;
typedef uint32_t ino_t;
typedef uint32_t nlink_t;
typedef uint32_t blksize_t;
typedef uint32_t blkcnt_t;
typedef int32_t  time_t;

#endif
