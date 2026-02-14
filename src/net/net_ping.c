/*
 * Kernel-level ICMP ping test using lwIP raw API.
 * Sends ICMP echo requests to 10.0.2.2 (QEMU user-mode gateway)
 * and logs results to the serial console.
 *
 * All raw API calls are executed inside the tcpip thread via
 * tcpip_callback(), as required by lwIP's threading model.
 */
#include "lwip/opt.h"
#include "lwip/raw.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/inet_chksum.h"
#include "lwip/tcpip.h"
#include "lwip/prot/icmp.h"
#include "lwip/def.h"

#include "sync.h"
#include "console.h"
#include "process.h"
#include "net.h"
#include "e1000.h"
#include "timer.h"

#include <stdint.h>
#include <stddef.h>

#define PING_ID         0xAD05
#define PING_COUNT      3
#define PING_TIMEOUT_MS 3000

extern uint32_t get_tick_count(void);

/* ---- Shared state between ping thread and tcpip callbacks ---- */

static struct raw_pcb *g_ping_pcb;
static ksem_t          ping_reply_sem;
static ksem_t          ping_setup_sem;
static volatile int    ping_got_reply;
static volatile uint16_t ping_reply_seqno;

/* ---- Raw receive callback (runs in tcpip thread) ---- */

static u8_t ping_recv_cb(void *arg, struct raw_pcb *pcb,
                          struct pbuf *p, const ip_addr_t *addr) {
    (void)arg; (void)pcb; (void)addr;

    /* Minimum: IP header (≥20) + ICMP echo header (8) */
    if (p->tot_len < 28)
        return 0;

    /* Read IP header first byte to determine IHL */
    uint8_t ihl_byte;
    if (pbuf_copy_partial(p, &ihl_byte, 1, 0) != 1)
        return 0;
    u16_t ip_hdr_len = (u16_t)((ihl_byte & 0x0F) * 4);

    struct icmp_echo_hdr echo;
    if (pbuf_copy_partial(p, &echo, sizeof(echo), ip_hdr_len) != sizeof(echo))
        return 0;

    if (echo.type == ICMP_ER && echo.id == PP_HTONS(PING_ID)) {
        ping_reply_seqno = lwip_ntohs(echo.seqno);
        ping_got_reply = 1;
        ksem_signal(&ping_reply_sem);
        pbuf_free(p);
        return 1; /* consumed */
    }

    return 0; /* not ours */
}

/* ---- tcpip_callback helpers ---- */

static void ping_setup_tcpip(void *arg) {
    (void)arg;
    g_ping_pcb = raw_new(IP_PROTO_ICMP);
    if (g_ping_pcb) {
        raw_recv(g_ping_pcb, ping_recv_cb, NULL);
        raw_bind(g_ping_pcb, IP_ADDR_ANY);
    }
    ksem_signal(&ping_setup_sem);
}

static void ping_cleanup_tcpip(void *arg) {
    (void)arg;
    if (g_ping_pcb) {
        raw_remove(g_ping_pcb);
        g_ping_pcb = NULL;
    }
    ksem_signal(&ping_setup_sem);
}

struct ping_send_ctx {
    ip_addr_t target;
    uint16_t  seq;
};

static struct ping_send_ctx g_send_ctx;

static void ping_send_tcpip(void *arg) {
    struct ping_send_ctx *ctx = (struct ping_send_ctx *)arg;

    struct pbuf *p = pbuf_alloc(PBUF_IP, (u16_t)sizeof(struct icmp_echo_hdr),
                                PBUF_RAM);
    if (!p) return;

    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
    iecho->type   = ICMP_ECHO;
    iecho->code   = 0;
    iecho->chksum = 0;
    iecho->id     = PP_HTONS(PING_ID);
    iecho->seqno  = lwip_htons(ctx->seq);
    iecho->chksum = inet_chksum(iecho, (u16_t)sizeof(*iecho));

    raw_sendto(g_ping_pcb, p, &ctx->target);
    pbuf_free(p);
}

/* ---- Public API ---- */

void net_ping_test(void) {
    if (!net_get_netif()) return;

    ksem_init(&ping_reply_sem, 0);
    ksem_init(&ping_setup_sem, 0);

    /* Create raw PCB in tcpip thread, wait for completion */
    g_ping_pcb = NULL;
    tcpip_callback(ping_setup_tcpip, NULL);
    ksem_wait(&ping_setup_sem);

    if (!g_ping_pcb) {
        kprintf("[PING] failed to create raw PCB\n");
        return;
    }

    /* Wait for the E1000 link to stabilize in QEMU */
    process_sleep(2 * TIMER_HZ); /* ~2 seconds */

    ip_addr_t target;
    IP4_ADDR(&target, 10, 0, 2, 2);

    int ok = 0;
    for (int i = 0; i < PING_COUNT; i++) {
        ping_got_reply = 0;

        g_send_ctx.target = target;
        g_send_ctx.seq    = (uint16_t)(i + 1);

        uint32_t t0 = get_tick_count();
        tcpip_callback(ping_send_tcpip, &g_send_ctx);

        /* Active poll: call e1000_recv + feed lwIP from this thread
         * while waiting for the reply.  This avoids depending on
         * the rx_thread being scheduled in time. */
        uint32_t deadline = t0 + (PING_TIMEOUT_MS + 19) / 20;
        while (!ping_got_reply && get_tick_count() < deadline) {
            static uint8_t ping_rx_buf[2048];
            int len = e1000_recv(ping_rx_buf, sizeof(ping_rx_buf));
            if (len > 0) {
                struct pbuf* p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
                if (p) {
                    pbuf_take(p, ping_rx_buf, (u16_t)len);
                    if (net_get_netif()->input(p, net_get_netif()) != ERR_OK)
                        pbuf_free(p);
                }
            }
            process_sleep(1); /* yield for 1 tick */
        }

        uint32_t dt = (get_tick_count() - t0) * TIMER_MS_PER_TICK;

        if (ping_got_reply) {
            kprintf("[PING] reply from 10.0.2.2: seq=%d time=%dms\n",
                    i + 1, dt);
            ok++;
        } else {
            kprintf("[PING] timeout seq=%d\n", i + 1);
        }

        if (i + 1 < PING_COUNT)
            process_sleep(TIMER_HZ); /* ~1 second between pings */
    }

    /* Cleanup in tcpip thread */
    tcpip_callback(ping_cleanup_tcpip, NULL);
    ksem_wait(&ping_setup_sem);

    if (ok > 0) {
        kprintf("[PING] %d/%d received — network OK\n", ok, PING_COUNT);
    } else {
        kprintf("[PING] all packets lost — network FAIL\n");
    }
}
