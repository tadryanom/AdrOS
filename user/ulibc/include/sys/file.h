// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_FILE_H
#define ULIBC_SYS_FILE_H

#define LOCK_SH  1   /* shared lock */
#define LOCK_EX  2   /* exclusive lock */
#define LOCK_NB  4   /* non-blocking */
#define LOCK_UN  8   /* unlock */

int flock(int fd, int operation);

#endif
