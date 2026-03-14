// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* Per-process errno: each fork()ed process gets its own copy in its
   address space.  When true threads (clone) are added, this must become
   __thread int errno or use a TLS segment (GS/FS). */
int errno = 0;
