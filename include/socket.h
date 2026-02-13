#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stddef.h>

/* Address families */
#define AF_INET     2

/* Socket types */
#define SOCK_STREAM 1   /* TCP */
#define SOCK_DGRAM  2   /* UDP */

/* Protocols */
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

/* Shutdown how */
#define SHUT_RD     0
#define SHUT_WR     1
#define SHUT_RDWR   2

/* Socket options */
#define SOL_SOCKET      1
#define SO_REUSEADDR    2
#define SO_ERROR        4
#define SO_KEEPALIVE    9

/* sockaddr_in (IPv4) — matches POSIX layout */
struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;      /* network byte order */
    uint32_t sin_addr;      /* network byte order */
    uint8_t  sin_zero[8];
};

/* Generic sockaddr */
struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

typedef uint32_t socklen_t;

/* Max kernel sockets */
#define KSOCKET_MAX         16
#define KSOCKET_RX_BUF_SIZE 4096
#define KSOCKET_ACCEPT_MAX  4

/* Kernel socket states */
#define KSOCK_CLOSED      0
#define KSOCK_CREATED     1
#define KSOCK_BOUND       2
#define KSOCK_LISTENING   3
#define KSOCK_CONNECTING  4
#define KSOCK_CONNECTED   5
#define KSOCK_PEER_CLOSED 6

/* Byte order helpers (x86 is little-endian) */
static inline uint16_t htons(uint16_t x) { return (uint16_t)((x >> 8) | (x << 8)); }
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000U);
}
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }

/* Kernel socket API (called from syscall layer) */
int  ksocket_create(int domain, int type, int protocol);
int  ksocket_bind(int sid, const struct sockaddr_in* addr);
int  ksocket_listen(int sid, int backlog);
int  ksocket_accept(int sid, struct sockaddr_in* addr);
int  ksocket_connect(int sid, const struct sockaddr_in* addr);
int  ksocket_send(int sid, const void* buf, size_t len, int flags);
int  ksocket_recv(int sid, void* buf, size_t len, int flags);
int  ksocket_sendto(int sid, const void* buf, size_t len, int flags,
                    const struct sockaddr_in* dest);
int  ksocket_recvfrom(int sid, void* buf, size_t len, int flags,
                      struct sockaddr_in* src);
int  ksocket_close(int sid);
int  ksocket_poll(int sid, int events);
void ksocket_init(void);

#endif
