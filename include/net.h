#ifndef NET_H
#define NET_H

struct netif;

/* Initialize the network stack (lwIP + E1000 netif). */
void net_init(void);

/* Poll for received packets and process lwIP timeouts. Call periodically. */
void net_poll(void);

/* Get the active network interface (or NULL). */
struct netif* net_get_netif(void);

/* Run ICMP ping test (sends echo requests to QEMU gateway 10.0.2.2). */
void net_ping_test(void);

#endif
