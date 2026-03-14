// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _NETPACKET_PACKET_H
#define _NETPACKET_PACKET_H
struct sockaddr_ll {
    unsigned short sll_family; unsigned short sll_protocol;
    int sll_ifindex; unsigned short sll_hatype;
    unsigned char sll_pkttype; unsigned char sll_halen;
    unsigned char sll_addr[8];
};
#define PACKET_HOST 0
#define PACKET_BROADCAST 1
#define PACKET_MULTICAST 2
#endif
