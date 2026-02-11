#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- NO_SYS mode (raw API, no threads) ---- */
#define NO_SYS                  1
#define LWIP_SOCKET             0
#define LWIP_NETCONN            0
#define LWIP_NETIF_API          0

/* ---- Memory settings ---- */
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (64 * 1024)     /* 64 KB heap for lwIP */
#define MEMP_NUM_PBUF           32
#define MEMP_NUM_UDP_PCB        8
#define MEMP_NUM_TCP_PCB        8
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG        32
#define MEMP_NUM_ARP_QUEUE      8
#define PBUF_POOL_SIZE          32
#define PBUF_POOL_BUFSIZE       1536

/* ---- IPv4 ---- */
#define LWIP_IPV4               1
#define LWIP_IPV6               0
#define LWIP_ARP                1
#define LWIP_ICMP               1
#define LWIP_UDP                1
#define LWIP_TCP                1
#define LWIP_DHCP               0
#define LWIP_AUTOIP             0
#define LWIP_DNS                0
#define LWIP_IGMP               0

/* ---- TCP tuning ---- */
#define TCP_MSS                 1460
#define TCP_WND                 (4 * TCP_MSS)
#define TCP_SND_BUF             (4 * TCP_MSS)
#define TCP_SND_QUEUELEN        (4 * TCP_SND_BUF / TCP_MSS)
#define TCP_QUEUE_OOSEQ         1

/* ---- Checksum ---- */
#define CHECKSUM_GEN_IP         1
#define CHECKSUM_GEN_UDP        1
#define CHECKSUM_GEN_TCP        1
#define CHECKSUM_GEN_ICMP       1
#define CHECKSUM_CHECK_IP       1
#define CHECKSUM_CHECK_UDP      1
#define CHECKSUM_CHECK_TCP      1
#define CHECKSUM_CHECK_ICMP     1

/* ---- ARP ---- */
#define ARP_TABLE_SIZE          10
#define ARP_QUEUEING            1
#define ETHARP_SUPPORT_STATIC_ENTRIES 1

/* ---- Debug (all off) ---- */
#define LWIP_DEBUG              0
#define LWIP_DBG_TYPES_ON       0

/* ---- Misc ---- */
#define LWIP_STATS              0
#define LWIP_PROVIDE_ERRNO      0
#define LWIP_RAND()             ((u32_t)0x12345678)  /* TODO: proper RNG */
#define LWIP_TIMERS             1
#define SYS_LIGHTWEIGHT_PROT    0
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS 1

/* ---- Raw API callbacks ---- */
#define LWIP_RAW                1
#define MEMP_NUM_RAW_PCB        4

#endif /* LWIPOPTS_H */
