// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef NET_H
#define NET_H

struct netif;

/* Initialize the network stack (lwIP + E1000 netif). */
void net_init(void);

/* Poll for received packets and process lwIP timeouts. Call periodically. */
void net_poll(void);

/* Get the active network interface (or NULL). */
struct netif* net_get_netif(void);

/* Start DHCP client on the active network interface. */
void net_dhcp_start(void);

/* Run ICMP ping test (sends echo requests to QEMU gateway 10.0.2.2). */
void net_ping_test(void);

#endif
