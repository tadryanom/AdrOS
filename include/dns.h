// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef DNS_H
#define DNS_H

#include <stdint.h>

void dns_resolver_init(uint32_t server_ip);
int  dns_resolve(const char* hostname, uint32_t* out_ip);

#endif
