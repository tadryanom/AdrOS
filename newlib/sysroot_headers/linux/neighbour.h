// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _LINUX_NEIGHBOUR_H
#define _LINUX_NEIGHBOUR_H
struct ndmsg { unsigned char ndm_family; int ndm_ifindex; uint16_t ndm_state; uint8_t ndm_flags; uint8_t ndm_type; };
#define NDA_DST 1
#define NDA_LLADDR 2
#define NUD_INCOMPLETE 0x01
#define NUD_REACHABLE 0x02
#define NUD_STALE 0x04
#endif
