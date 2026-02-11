// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* Per-process errno: each fork()ed process gets its own copy in its
   address space.  When true threads (clone) are added, this must become
   __thread int errno or use a TLS segment (GS/FS). */
int errno = 0;
