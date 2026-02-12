// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef DNS_H
#define DNS_H

#include <stdint.h>

void dns_resolver_init(uint32_t server_ip);
int  dns_resolve(const char* hostname, uint32_t* out_ip);

#endif
