#ifndef DNS_H
#define DNS_H

#include <stdint.h>

void dns_resolver_init(uint32_t server_ip);
int  dns_resolve(const char* hostname, uint32_t* out_ip);

#endif
