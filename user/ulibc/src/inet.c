// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "arpa/inet.h"
#include "string.h"
#include "errno.h"

in_addr_t inet_addr(const char* cp) {
    struct in_addr addr;
    if (inet_aton(cp, &addr))
        return addr.s_addr;
    return INADDR_NONE;
}

int inet_aton(const char* cp, struct in_addr* inp) {
    uint32_t val = 0;
    int parts = 0;
    uint32_t part = 0;
    for (const char* p = cp; ; p++) {
        if (*p >= '0' && *p <= '9') {
            part = part * 10 + (unsigned)(*p - '0');
            if (part > 255) return 0;
        } else if (*p == '.' || *p == '\0') {
            val = (val << 8) | part;
            part = 0;
            parts++;
            if (*p == '\0') break;
            if (parts >= 4) return 0;
        } else {
            return 0;
        }
    }
    if (parts != 4) return 0;
    if (inp) inp->s_addr = htonl(val);
    return 1;
}

static char _ntoa_buf[16];
char* inet_ntoa(struct in_addr in) {
    uint32_t addr = ntohl(in.s_addr);
    int pos = 0;
    for (int i = 3; i >= 0; i--) {
        unsigned int octet = (addr >> (i * 8)) & 0xFF;
        if (octet >= 100) _ntoa_buf[pos++] = '0' + (char)(octet / 100);
        if (octet >= 10) _ntoa_buf[pos++] = '0' + (char)((octet / 10) % 10);
        _ntoa_buf[pos++] = '0' + (char)(octet % 10);
        if (i > 0) _ntoa_buf[pos++] = '.';
    }
    _ntoa_buf[pos] = '\0';
    return _ntoa_buf;
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    if (af == AF_INET) {
        struct in_addr a;
        memcpy(&a, src, sizeof(a));
        char* s = inet_ntoa(a);
        size_t len = strlen(s);
        if (len >= size) { errno = ENOSPC; return (void*)0; }
        memcpy(dst, s, len + 1);
        return dst;
    }
    errno = EAFNOSUPPORT;
    return (void*)0;
}

int inet_pton(int af, const char* src, void* dst) {
    if (af == AF_INET) {
        struct in_addr addr;
        if (!inet_aton(src, &addr)) return 0;
        memcpy(dst, &addr, sizeof(addr));
        return 1;
    }
    errno = EAFNOSUPPORT;
    return -1;
}
