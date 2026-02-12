#include "dns.h"
#include "uart_console.h"

#include "lwip/dns.h"
#include "lwip/ip_addr.h"

void dns_resolver_init(uint32_t server_ip) {
    dns_init();
    ip_addr_t dns_server;
    IP4_ADDR(&dns_server, (server_ip >> 24) & 0xFF,
                          (server_ip >> 16) & 0xFF,
                          (server_ip >> 8) & 0xFF,
                          server_ip & 0xFF);
    dns_setserver(0, &dns_server);
    uart_print("[DNS] Resolver initialized\n");
}

static volatile int dns_done;
static volatile uint32_t dns_result_ip;

static void dns_found_cb(const char* name, const ip_addr_t* ipaddr, void* arg) {
    (void)name;
    (void)arg;
    if (ipaddr) {
        dns_result_ip = ip4_addr_get_u32(ip_2_ip4(ipaddr));
    } else {
        dns_result_ip = 0;
    }
    dns_done = 1;
}

int dns_resolve(const char* hostname, uint32_t* out_ip) {
    if (!hostname || !out_ip) return -1;

    ip_addr_t resolved;
    dns_done = 0;
    dns_result_ip = 0;

    err_t err = dns_gethostbyname(hostname, &resolved, dns_found_cb, 0);
    if (err == ERR_OK) {
        /* Already cached */
        *out_ip = ip4_addr_get_u32(ip_2_ip4(&resolved));
        return 0;
    }
    if (err == ERR_INPROGRESS) {
        /* Wait for callback — simple busy-wait with timeout */
        extern uint32_t get_tick_count(void);
        uint32_t deadline = get_tick_count() + 250; /* 5 second timeout at 50Hz */
        while (!dns_done && get_tick_count() < deadline) {
            /* Let lwIP process packets */
            extern void schedule(void);
            schedule();
        }
        if (dns_done && dns_result_ip != 0) {
            *out_ip = dns_result_ip;
            return 0;
        }
        return -2; /* timeout or NXDOMAIN */
    }

    return -1; /* error */
}
