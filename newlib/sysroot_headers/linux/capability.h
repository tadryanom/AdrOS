// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _LINUX_CAPABILITY_H
#define _LINUX_CAPABILITY_H
#include <stdint.h>
typedef struct { uint32_t version; int pid; } cap_header_t;
typedef struct { uint32_t effective; uint32_t permitted; uint32_t inheritable; } cap_data_t;
#define CAP_NET_RAW 13
#define CAP_SYS_ADMIN 21
#endif
