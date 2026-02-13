/*
 * lwIP netif driver for the E1000 NIC.
 * Bridges the E1000 hardware driver to lwIP's network interface abstraction.
 *
 * RX path:  interrupt → e1000_rx_sem → rx_thread → e1000_recv → tcpip_input
 * TX path:  lwIP core → e1000_netif_output → e1000_send (non-blocking)
 */
#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "lwip/sys.h"
#include "netif/ethernet.h"
#include "spinlock.h"

#include "e1000.h"
#include "process.h"
#include "console.h"
#include "utils.h"

#define E1000_NETIF_MTU  1500

/* Forward declaration */
static err_t e1000_netif_output(struct netif* netif, struct pbuf* p);

/*
 * Low-level output: send a pbuf chain via E1000 (non-blocking).
 */
static err_t e1000_netif_output(struct netif* netif, struct pbuf* p) {
    (void)netif;
    if (!p) return ERR_ARG;

    /* Flatten pbuf chain into a contiguous buffer */
    uint16_t total = (uint16_t)p->tot_len;
    if (total > E1000_TX_BUF_SIZE) return ERR_MEM;

    /* If single pbuf, send directly */
    if (p->next == NULL) {
        if (e1000_send(p->payload, total) < 0)
            return ERR_IF;
        return ERR_OK;
    }

    /* Multi-segment: copy to temp buffer */
    static uint8_t tx_tmp[2048];
    uint16_t off = 0;
    for (struct pbuf* q = p; q != NULL; q = q->next) {
        if (off + q->len > sizeof(tx_tmp)) return ERR_MEM;
        memcpy(tx_tmp + off, q->payload, q->len);
        off += q->len;
    }

    if (e1000_send(tx_tmp, total) < 0)
        return ERR_IF;

    return ERR_OK;
}

/*
 * Netif init callback — called by netif_add().
 */
static err_t e1000_netif_init(struct netif* netif) {
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output = etharp_output;
    netif->linkoutput = e1000_netif_output;
    netif->mtu = E1000_NETIF_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->hwaddr_len = 6;

    e1000_get_mac(netif->hwaddr);

    return ERR_OK;
}

/* ---- Global network state ---- */

static struct netif e1000_nif;
static int net_initialized = 0;

static volatile int tcpip_ready = 0;

static void net_init_done(void* arg) {
    (void)arg;
    tcpip_ready = 1;
}

/*
 * Dedicated RX thread — waits on the E1000 RX semaphore (signaled by
 * the hardware interrupt handler) and drains all available packets into
 * the lwIP TCP/IP stack via tcpip_input().
 */
static uint8_t rx_tmp[2048];

static void e1000_rx_thread(void) {
    for (;;) {
        /* Block until the IRQ handler signals a receive event */
        ksem_wait(&e1000_rx_sem);

        /* Drain all available packets */
        for (;;) {
            int len = e1000_recv(rx_tmp, sizeof(rx_tmp));
            if (len <= 0) break;

            struct pbuf* p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
            if (!p) break;

            pbuf_take(p, rx_tmp, (u16_t)len);

            if (e1000_nif.input(p, &e1000_nif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
}

void net_init(void) {
    if (!e1000_link_up()) {
        kprintf("[NET] E1000 link down, skipping lwIP init.\n");
        return;
    }

    /* Start lwIP tcpip thread and poll until it signals ready */
    tcpip_init(net_init_done, NULL);
    while (!tcpip_ready) {
        cpu_relax();
    }

    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr,  10, 0, 2, 15);   /* QEMU user-mode default */
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      10, 0, 2, 2);    /* QEMU user-mode gateway */

    netif_add(&e1000_nif, &ipaddr, &netmask, &gw, NULL,
              e1000_netif_init, tcpip_input);
    netif_set_default(&e1000_nif);
    netif_set_up(&e1000_nif);

    /* Start the dedicated RX thread */
    process_create_kernel(e1000_rx_thread);

    net_initialized = 1;

    kprintf("[NET] lwIP initialized (interrupt-driven RX), IP=10.0.2.15\n");
}

void net_poll(void) {
    /* No-op: RX is now handled by the interrupt-driven rx_thread.
     * Kept for backward compatibility — callers can safely remove. */
    (void)0;
}

struct netif* net_get_netif(void) {
    return net_initialized ? &e1000_nif : NULL;
}
