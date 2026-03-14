// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_SOCKET_H
#define ULIBC_SYS_SOCKET_H

#include <stdint.h>
#include <stddef.h>

/* Address families */
#define AF_UNSPEC   0
#define AF_UNIX     1
#define AF_LOCAL    AF_UNIX
#define AF_INET     2
#define AF_INET6    10

/* Socket types */
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_NONBLOCK  0x800
#define SOCK_CLOEXEC   0x80000

/* Protocol families (aliases) */
#define PF_UNSPEC   AF_UNSPEC
#define PF_UNIX     AF_UNIX
#define PF_INET     AF_INET
#define PF_INET6    AF_INET6

/* Socket options levels */
#define SOL_SOCKET  1

/* Socket options */
#define SO_REUSEADDR  2
#define SO_ERROR      4
#define SO_BROADCAST  6
#define SO_SNDBUF     7
#define SO_RCVBUF     8
#define SO_KEEPALIVE  9
#define SO_LINGER     13
#define SO_RCVTIMEO   20
#define SO_SNDTIMEO   21

/* shutdown() how */
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

/* MSG flags */
#define MSG_PEEK       0x02
#define MSG_DONTWAIT   0x40
#define MSG_NOSIGNAL   0x4000

typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char        __ss_pad[126];
};

struct msghdr {
    void*        msg_name;
    socklen_t    msg_namelen;
    struct iovec* msg_iov;
    int          msg_iovlen;
    void*        msg_control;
    socklen_t    msg_controllen;
    int          msg_flags;
};

struct iovec;

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int send(int sockfd, const void* buf, size_t len, int flags);
int recv(int sockfd, void* buf, size_t len, int flags);
int sendto(int sockfd, const void* buf, size_t len, int flags,
           const struct sockaddr* dest_addr, socklen_t addrlen);
int recvfrom(int sockfd, void* buf, size_t len, int flags,
             struct sockaddr* src_addr, socklen_t* addrlen);
int sendmsg(int sockfd, const struct msghdr* msg, int flags);
int recvmsg(int sockfd, struct msghdr* msg, int flags);
int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen);
int shutdown(int sockfd, int how);
int getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

#endif
