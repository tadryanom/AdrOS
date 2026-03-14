// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "netdb.h"
#include "syscall.h"
#include "errno.h"
#include "stdlib.h"
#include "string.h"

int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** res) {
    (void)service;
    if (!res) return EAI_FAIL;

    /* Use kernel getaddrinfo syscall for DNS resolution */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    int ret = _syscall3(SYS_GETADDRINFO, (int)node, (int)&addr.sin_addr, 0);
    if (ret < 0) return EAI_NONAME;

    struct addrinfo* ai = (struct addrinfo*)calloc(1, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    if (!ai) return EAI_MEMORY;

    struct sockaddr_in* sa = (struct sockaddr_in*)(ai + 1);
    memcpy(sa, &addr, sizeof(struct sockaddr_in));

    ai->ai_family = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_addr = (struct sockaddr*)sa;
    ai->ai_next = (void*)0;
    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo* res) {
    while (res) {
        struct addrinfo* next = res->ai_next;
        free(res);
        res = next;
    }
}

const char* gai_strerror(int errcode) {
    switch (errcode) {
    case 0: return "Success";
    case EAI_NONAME: return "Name does not resolve";
    case EAI_AGAIN: return "Temporary failure in name resolution";
    case EAI_FAIL: return "Non-recoverable failure in name resolution";
    case EAI_MEMORY: return "Memory allocation failure";
    case EAI_FAMILY: return "Address family not supported";
    case EAI_SOCKTYPE: return "Socket type not supported";
    case EAI_SERVICE: return "Service not supported";
    case EAI_SYSTEM: return "System error";
    default: return "Unknown error";
    }
}
