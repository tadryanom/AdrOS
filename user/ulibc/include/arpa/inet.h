// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_ARPA_INET_H
#define ULIBC_ARPA_INET_H

#include <stdint.h>
#include <netinet/in.h>

in_addr_t   inet_addr(const char* cp);
int         inet_aton(const char* cp, struct in_addr* inp);
char*       inet_ntoa(struct in_addr in);
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size);
int         inet_pton(int af, const char* src, void* dst);

#endif
