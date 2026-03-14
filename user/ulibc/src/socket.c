// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/socket.h"
#include "syscall.h"
#include "errno.h"

int socket(int domain, int type, int protocol) {
    return __syscall_ret(_syscall3(SYS_SOCKET, domain, type, protocol));
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return __syscall_ret(_syscall3(SYS_BIND, sockfd, (int)addr, (int)addrlen));
}

int listen(int sockfd, int backlog) {
    return __syscall_ret(_syscall2(SYS_LISTEN, sockfd, backlog));
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return __syscall_ret(_syscall3(SYS_ACCEPT, sockfd, (int)addr, (int)addrlen));
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return __syscall_ret(_syscall3(SYS_CONNECT, sockfd, (int)addr, (int)addrlen));
}

int send(int sockfd, const void* buf, size_t len, int flags) {
    return __syscall_ret(_syscall4(SYS_SEND, sockfd, (int)buf, (int)len, flags));
}

int recv(int sockfd, void* buf, size_t len, int flags) {
    return __syscall_ret(_syscall4(SYS_RECV, sockfd, (int)buf, (int)len, flags));
}

int sendto(int sockfd, const void* buf, size_t len, int flags,
           const struct sockaddr* dest_addr, socklen_t addrlen) {
    (void)addrlen;
    return __syscall_ret(_syscall5(SYS_SENDTO, sockfd, (int)buf, (int)len, flags, (int)dest_addr));
}

int recvfrom(int sockfd, void* buf, size_t len, int flags,
             struct sockaddr* src_addr, socklen_t* addrlen) {
    (void)addrlen;
    return __syscall_ret(_syscall5(SYS_RECVFROM, sockfd, (int)buf, (int)len, flags, (int)src_addr));
}

int sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    return __syscall_ret(_syscall3(SYS_SENDMSG, sockfd, (int)msg, flags));
}

int recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return __syscall_ret(_syscall3(SYS_RECVMSG, sockfd, (int)msg, flags));
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    return __syscall_ret(_syscall5(SYS_SETSOCKOPT, sockfd, level, optname, (int)optval, (int)optlen));
}

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
    return __syscall_ret(_syscall5(SYS_GETSOCKOPT, sockfd, level, optname, (int)optval, (int)optlen));
}

int shutdown(int sockfd, int how) {
    return __syscall_ret(_syscall2(SYS_SHUTDOWN, sockfd, how));
}

int getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return __syscall_ret(_syscall3(SYS_GETPEERNAME, sockfd, (int)addr, (int)addrlen));
}

int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return __syscall_ret(_syscall3(SYS_GETSOCKNAME, sockfd, (int)addr, (int)addrlen));
}
