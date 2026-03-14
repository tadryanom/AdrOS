// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_SENDFILE_H
#define _SYS_SENDFILE_H
#include <sys/types.h>
ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count);
#endif
