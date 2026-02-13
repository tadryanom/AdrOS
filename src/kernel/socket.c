#include "socket.h"
#include "net.h"
#include "errno.h"
#include "process.h"
#include "waitqueue.h"
#include "console.h"
#include "utils.h"

#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Kernel socket table                                                */
/* ------------------------------------------------------------------ */

struct ksocket {
    int      in_use;
    int      type;          /* SOCK_STREAM or SOCK_DGRAM */
    int      state;
    union {
        struct tcp_pcb* tcp;
        struct udp_pcb* udp;
    } pcb;

    /* Receive ring buffer */
    uint8_t  rx_buf[KSOCKET_RX_BUF_SIZE];
    uint16_t rx_head;
    uint16_t rx_tail;
    uint16_t rx_count;

    /* Accept queue (TCP listening sockets) */
    int      accept_queue[KSOCKET_ACCEPT_MAX];
    int      aq_head;
    int      aq_tail;
    int      aq_count;

    /* UDP recvfrom: last received source */
    uint32_t last_remote_ip;
    uint16_t last_remote_port;

    /* Wait queues */
    waitqueue_t rx_wq;
    waitqueue_t accept_wq;
    waitqueue_t connect_wq;

    int      error;
};

static struct ksocket sockets[KSOCKET_MAX];

void ksocket_init(void) {
    for (int i = 0; i < KSOCKET_MAX; i++) {
        sockets[i].in_use = 0;
    }
}

static int alloc_socket(void) {
    for (int i = 0; i < KSOCKET_MAX; i++) {
        if (!sockets[i].in_use) {
            memset(&sockets[i], 0, sizeof(struct ksocket));
            sockets[i].in_use = 1;
            sockets[i].state = KSOCK_CREATED;
            wq_init(&sockets[i].rx_wq);
            wq_init(&sockets[i].accept_wq);
            wq_init(&sockets[i].connect_wq);
            return i;
        }
    }
    return -ENOMEM;
}

static struct ksocket* get_socket(int sid) {
    if (sid < 0 || sid >= KSOCKET_MAX) return NULL;
    if (!sockets[sid].in_use) return NULL;
    return &sockets[sid];
}

/* ------------------------------------------------------------------ */
/* Ring buffer helpers                                                */
/* ------------------------------------------------------------------ */

static int rxbuf_write(struct ksocket* s, const void* data, uint16_t len) {
    const uint8_t* src = data;
    uint16_t avail = KSOCKET_RX_BUF_SIZE - s->rx_count;
    if (len > avail) len = avail;
    for (uint16_t i = 0; i < len; i++) {
        s->rx_buf[s->rx_tail] = src[i];
        s->rx_tail = (uint16_t)((s->rx_tail + 1) % KSOCKET_RX_BUF_SIZE);
        s->rx_count++;
    }
    return len;
}

static int rxbuf_read(struct ksocket* s, void* data, uint16_t len) {
    uint8_t* dst = data;
    if (len > s->rx_count) len = s->rx_count;
    for (uint16_t i = 0; i < len; i++) {
        dst[i] = s->rx_buf[s->rx_head];
        s->rx_head = (uint16_t)((s->rx_head + 1) % KSOCKET_RX_BUF_SIZE);
        s->rx_count--;
    }
    return len;
}

/* ------------------------------------------------------------------ */
/* lwIP TCP callbacks                                                 */
/* ------------------------------------------------------------------ */

static err_t tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    int sid = (int)(uintptr_t)arg;
    struct ksocket* s = get_socket(sid);
    if (!s) { if (p) pbuf_free(p); return ERR_ABRT; }

    if (!p || err != ERR_OK) {
        /* Peer closed */
        s->state = KSOCK_PEER_CLOSED;
        wq_wake_all(&s->rx_wq);
        if (p) pbuf_free(p);
        return ERR_OK;
    }

    /* Copy data into ring buffer */
    uint16_t copied = 0;
    for (struct pbuf* q = p; q != NULL; q = q->next) {
        copied += (uint16_t)rxbuf_write(s, q->payload, q->len);
    }
    tcp_recved(tpcb, copied);
    pbuf_free(p);

    wq_wake_all(&s->rx_wq);
    return ERR_OK;
}

static err_t tcp_connected_cb(void* arg, struct tcp_pcb* tpcb, err_t err) {
    (void)tpcb;
    int sid = (int)(uintptr_t)arg;
    struct ksocket* s = get_socket(sid);
    if (!s) return ERR_ABRT;

    if (err == ERR_OK) {
        s->state = KSOCK_CONNECTED;
    } else {
        s->error = -ECONNREFUSED;
        s->state = KSOCK_CLOSED;
    }
    wq_wake_all(&s->connect_wq);
    return ERR_OK;
}

static err_t tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err) {
    int sid = (int)(uintptr_t)arg;
    struct ksocket* s = get_socket(sid);
    if (!s || err != ERR_OK) return ERR_ABRT;

    if (s->aq_count >= KSOCKET_ACCEPT_MAX) {
        return ERR_MEM;  /* Reject — queue full */
    }

    /* Allocate a new ksocket for the accepted connection */
    int new_sid = alloc_socket();
    if (new_sid < 0) return ERR_MEM;

    struct ksocket* ns = &sockets[new_sid];
    ns->type = SOCK_STREAM;
    ns->state = KSOCK_CONNECTED;
    ns->pcb.tcp = newpcb;

    tcp_arg(newpcb, (void*)(uintptr_t)new_sid);
    tcp_recv(newpcb, tcp_recv_cb);

    /* Enqueue */
    s->accept_queue[s->aq_tail] = new_sid;
    s->aq_tail = (s->aq_tail + 1) % KSOCKET_ACCEPT_MAX;
    s->aq_count++;

    wq_wake_all(&s->accept_wq);
    return ERR_OK;
}

/* ------------------------------------------------------------------ */
/* lwIP UDP callback                                                  */
/* ------------------------------------------------------------------ */

static void udp_recv_cb(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                        const ip_addr_t* addr, u16_t port) {
    (void)upcb;
    int sid = (int)(uintptr_t)arg;
    struct ksocket* s = get_socket(sid);
    if (!s || !p) { if (p) pbuf_free(p); return; }

    s->last_remote_ip = ip_addr_get_ip4_u32(addr);
    s->last_remote_port = port;

    for (struct pbuf* q = p; q != NULL; q = q->next) {
        rxbuf_write(s, q->payload, q->len);
    }
    pbuf_free(p);

    wq_wake_all(&s->rx_wq);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int ksocket_create(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != AF_INET) return -EAFNOSUPPORT;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return -EPROTONOSUPPORT;

    int sid = alloc_socket();
    if (sid < 0) return sid;

    struct ksocket* s = &sockets[sid];
    s->type = type;

    if (type == SOCK_STREAM) {
        s->pcb.tcp = tcp_new();
        if (!s->pcb.tcp) { s->in_use = 0; return -ENOMEM; }
        tcp_arg(s->pcb.tcp, (void*)(uintptr_t)sid);
        tcp_recv(s->pcb.tcp, tcp_recv_cb);
    } else {
        s->pcb.udp = udp_new();
        if (!s->pcb.udp) { s->in_use = 0; return -ENOMEM; }
        udp_recv(s->pcb.udp, udp_recv_cb, (void*)(uintptr_t)sid);
    }

    return sid;
}

int ksocket_bind(int sid, const struct sockaddr_in* addr) {
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;

    ip_addr_t ip;
    ip_addr_set_ip4_u32(&ip, addr->sin_addr);
    uint16_t port = ntohs(addr->sin_port);

    err_t err;
    if (s->type == SOCK_STREAM) {
        err = tcp_bind(s->pcb.tcp, &ip, port);
    } else {
        err = udp_bind(s->pcb.udp, &ip, port);
    }

    if (err != ERR_OK) return -EADDRINUSE;
    s->state = KSOCK_BOUND;
    return 0;
}

int ksocket_listen(int sid, int backlog) {
    (void)backlog;
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;
    if (s->type != SOCK_STREAM) return -EOPNOTSUPP;

    struct tcp_pcb* lpcb = tcp_listen(s->pcb.tcp);
    if (!lpcb) return -ENOMEM;

    s->pcb.tcp = lpcb;
    s->state = KSOCK_LISTENING;
    tcp_arg(lpcb, (void*)(uintptr_t)sid);
    tcp_accept(lpcb, tcp_accept_cb);

    return 0;
}

int ksocket_accept(int sid, struct sockaddr_in* addr) {
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;
    if (s->state != KSOCK_LISTENING) return -EINVAL;

    /* Block until a connection arrives */
    while (s->aq_count == 0) {
        wq_push(&s->accept_wq, current_process);
        current_process->state = PROCESS_BLOCKED;
        schedule();
    }

    int new_sid = s->accept_queue[s->aq_head];
    s->aq_head = (s->aq_head + 1) % KSOCKET_ACCEPT_MAX;
    s->aq_count--;

    if (addr) {
        struct ksocket* ns = get_socket(new_sid);
        if (ns && ns->pcb.tcp) {
            addr->sin_family = AF_INET;
            addr->sin_port = htons(ns->pcb.tcp->remote_port);
            addr->sin_addr = ip_addr_get_ip4_u32(&ns->pcb.tcp->remote_ip);
        }
    }

    return new_sid;
}

int ksocket_connect(int sid, const struct sockaddr_in* addr) {
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;

    ip_addr_t ip;
    ip_addr_set_ip4_u32(&ip, addr->sin_addr);
    uint16_t port = ntohs(addr->sin_port);

    if (s->type == SOCK_STREAM) {
        s->state = KSOCK_CONNECTING;
        err_t err = tcp_connect(s->pcb.tcp, &ip, port, tcp_connected_cb);
        if (err != ERR_OK) return -ECONNREFUSED;

        /* Block until connected */
        while (s->state == KSOCK_CONNECTING) {
            wq_push(&s->connect_wq, current_process);
            current_process->state = PROCESS_BLOCKED;
            schedule();
        }
        if (s->state != KSOCK_CONNECTED) return s->error ? s->error : -ECONNREFUSED;
        return 0;
    } else {
        /* UDP "connect" just sets the default destination */
        udp_connect(s->pcb.udp, &ip, port);
        s->state = KSOCK_CONNECTED;
        return 0;
    }
}

int ksocket_send(int sid, const void* buf, size_t len, int flags) {
    (void)flags;
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;

    if (s->type == SOCK_STREAM) {
        if (s->state != KSOCK_CONNECTED) return -ENOTCONN;
        uint16_t snd_len = (len > 0xFFFF) ? 0xFFFF : (uint16_t)len;
        uint16_t avail = tcp_sndbuf(s->pcb.tcp);
        if (snd_len > avail) snd_len = avail;
        if (snd_len == 0) return -EAGAIN;

        err_t err = tcp_write(s->pcb.tcp, buf, snd_len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) return -EIO;
        tcp_output(s->pcb.tcp);
        return (int)snd_len;
    } else {
        /* UDP connected send */
        if (s->state != KSOCK_CONNECTED) return -ENOTCONN;
        struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
        if (!p) return -ENOMEM;
        memcpy(p->payload, buf, len);
        err_t err = udp_send(s->pcb.udp, p);
        pbuf_free(p);
        return (err == ERR_OK) ? (int)len : -EIO;
    }
}

int ksocket_recv(int sid, void* buf, size_t len, int flags) {
    (void)flags;
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;

    /* Block until data available or peer closed */
    while (s->rx_count == 0 && s->state != KSOCK_PEER_CLOSED && s->state != KSOCK_CLOSED) {
        wq_push(&s->rx_wq, current_process);
        current_process->state = PROCESS_BLOCKED;
        schedule();
    }

    if (s->rx_count == 0) return 0;  /* EOF / peer closed */

    uint16_t rlen = (len > 0xFFFF) ? 0xFFFF : (uint16_t)len;
    return rxbuf_read(s, buf, rlen);
}

int ksocket_sendto(int sid, const void* buf, size_t len, int flags,
                   const struct sockaddr_in* dest) {
    (void)flags;
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;
    if (s->type != SOCK_DGRAM) return -EOPNOTSUPP;

    ip_addr_t ip;
    ip_addr_set_ip4_u32(&ip, dest->sin_addr);
    uint16_t port = ntohs(dest->sin_port);

    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (!p) return -ENOMEM;
    memcpy(p->payload, buf, len);
    err_t err = udp_sendto(s->pcb.udp, p, &ip, port);
    pbuf_free(p);
    return (err == ERR_OK) ? (int)len : -EIO;
}

int ksocket_recvfrom(int sid, void* buf, size_t len, int flags,
                     struct sockaddr_in* src) {
    int ret = ksocket_recv(sid, buf, len, flags);
    if (ret > 0 && src) {
        struct ksocket* s = get_socket(sid);
        if (s) {
            src->sin_family = AF_INET;
            src->sin_port = htons(s->last_remote_port);
            src->sin_addr = s->last_remote_ip;
        }
    }
    return ret;
}

int ksocket_close(int sid) {
    struct ksocket* s = get_socket(sid);
    if (!s) return -EBADF;

    if (s->type == SOCK_STREAM && s->pcb.tcp) {
        tcp_arg(s->pcb.tcp, NULL);
        tcp_recv(s->pcb.tcp, NULL);
        tcp_accept(s->pcb.tcp, NULL);
        tcp_close(s->pcb.tcp);
    } else if (s->type == SOCK_DGRAM && s->pcb.udp) {
        udp_remove(s->pcb.udp);
    }

    /* Free any pending accepted sockets */
    while (s->aq_count > 0) {
        int child = s->accept_queue[s->aq_head];
        s->aq_head = (s->aq_head + 1) % KSOCKET_ACCEPT_MAX;
        s->aq_count--;
        ksocket_close(child);
    }

    wq_wake_all(&s->rx_wq);
    wq_wake_all(&s->accept_wq);
    wq_wake_all(&s->connect_wq);

    s->in_use = 0;
    return 0;
}
