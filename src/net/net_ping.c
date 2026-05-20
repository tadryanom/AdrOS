// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/*
 * Kernel-level ICMP ping test — REMOVED.
 *
 * net_ping_test() was a boot-time kernel ping test that sent ICMP echo
 * requests to 10.0.2.2 (QEMU gateway).  It has been superseded by the
 * userspace ICMP ping test in fulltest (I7b) which uses SOCK_RAW +
 * IPPROTO_ICMP via the standard socket API.
 *
 * This file is kept as a placeholder to avoid breaking the build
 * system's wildcard source enumeration in the Makefile.
 */
