#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- Threaded mode (full API with threads) ---- */
#define NO_SYS                  0
#define LWIP_SOCKET             0
#define LWIP_NETCONN            1
#define LWIP_NETIF_API          0
#define LWIP_COMPAT_MUTEX       0
#define LWIP_TCPIP_CORE_LOCKING 0
#define TCPIP_THREAD_STACKSIZE  4096
#define TCPIP_THREAD_PRIO       1
#define TCPIP_MBOX_SIZE         16
#define DEFAULT_THREAD_STACKSIZE 4096
#define DEFAULT_ACCEPTMBOX_SIZE 8
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8

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
#define LWIP_IPV6               1
#define LWIP_IPV6_MLD           1
#define LWIP_IPV6_AUTOCONFIG    1
#define LWIP_ICMP6              1
#define LWIP_IPV6_DHCP6         0
#define MEMP_NUM_MLD6_GROUP     4
#define LWIP_IPV6_NUM_ADDRESSES 3
#define LWIP_ND6_NUM_NEIGHBORS  8
#define LWIP_ND6_NUM_DESTINATIONS 8
#define LWIP_ND6_NUM_PREFIXES   4
#define LWIP_ND6_NUM_ROUTERS    2
#define LWIP_ARP                1
#define LWIP_ICMP               1
#define LWIP_UDP                1
#define LWIP_TCP                1
#define LWIP_DHCP               1
#define LWIP_AUTOIP             0
#define LWIP_DNS                1
#define LWIP_NO_CTYPE_H         1
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
#define SYS_LIGHTWEIGHT_PROT    1
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS 1

/* ---- Raw API callbacks ---- */
#define LWIP_RAW                1
#define MEMP_NUM_RAW_PCB        4

#endif /* LWIPOPTS_H */
