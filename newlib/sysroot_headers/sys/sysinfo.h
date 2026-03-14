// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H
struct sysinfo {
    long uptime; unsigned long loads[3]; unsigned long totalram;
    unsigned long freeram; unsigned long sharedram; unsigned long bufferram;
    unsigned long totalswap; unsigned long freeswap;
    unsigned short procs; unsigned long totalhigh; unsigned long freehigh;
    unsigned int mem_unit;
};
int sysinfo(struct sysinfo* info);
#endif
