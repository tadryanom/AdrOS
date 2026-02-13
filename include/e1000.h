#ifndef E1000_H
#define E1000_H

#include <stdint.h>

/* Intel 82540EM (E1000) PCI IDs */
#define E1000_VENDOR_ID  0x8086
#define E1000_DEVICE_ID  0x100E

/* E1000 Register Offsets */
#define E1000_CTRL       0x0000  /* Device Control */
#define E1000_STATUS     0x0008  /* Device Status */
#define E1000_EERD       0x0014  /* EEPROM Read */
#define E1000_ICR        0x00C0  /* Interrupt Cause Read */
#define E1000_ICS        0x00C4  /* Interrupt Cause Set */
#define E1000_IMS        0x00C8  /* Interrupt Mask Set */
#define E1000_IMC        0x00D0  /* Interrupt Mask Clear */
#define E1000_RCTL       0x0100  /* Receive Control */
#define E1000_TCTL       0x0400  /* Transmit Control */
#define E1000_TIPG       0x0410  /* TX Inter-Packet Gap */
#define E1000_RDBAL      0x2800  /* RX Descriptor Base Low */
#define E1000_RDBAH      0x2804  /* RX Descriptor Base High */
#define E1000_RDLEN      0x2808  /* RX Descriptor Length */
#define E1000_RDH        0x2810  /* RX Descriptor Head */
#define E1000_RDT        0x2818  /* RX Descriptor Tail */
#define E1000_TDBAL      0x3800  /* TX Descriptor Base Low */
#define E1000_TDBAH      0x3804  /* TX Descriptor Base High */
#define E1000_TDLEN      0x3808  /* TX Descriptor Length */
#define E1000_TDH        0x3810  /* TX Descriptor Head */
#define E1000_TDT        0x3818  /* TX Descriptor Tail */
#define E1000_MTA        0x5200  /* Multicast Table Array (128 entries) */
#define E1000_RAL0       0x5400  /* Receive Address Low */
#define E1000_RAH0       0x5404  /* Receive Address High */

/* CTRL bits */
#define E1000_CTRL_FD    (1U << 0)   /* Full Duplex */
#define E1000_CTRL_ASDE  (1U << 5)   /* Auto-Speed Detection Enable */
#define E1000_CTRL_SLU   (1U << 6)   /* Set Link Up */
#define E1000_CTRL_RST   (1U << 26)  /* Device Reset */

/* RCTL bits */
#define E1000_RCTL_EN    (1U << 1)   /* Receiver Enable */
#define E1000_RCTL_SBP   (1U << 2)   /* Store Bad Packets */
#define E1000_RCTL_UPE   (1U << 3)   /* Unicast Promiscuous */
#define E1000_RCTL_MPE   (1U << 4)   /* Multicast Promiscuous */
#define E1000_RCTL_LBM   (3U << 6)   /* Loopback Mode */
#define E1000_RCTL_BAM   (1U << 15)  /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048 (0U << 16) /* Buffer Size 2048 */
#define E1000_RCTL_BSIZE_4096 (3U << 16 | 1U << 25) /* Buffer Size 4096 (BSEX) */
#define E1000_RCTL_SECRC (1U << 26)  /* Strip Ethernet CRC */

/* TCTL bits */
#define E1000_TCTL_EN    (1U << 1)   /* Transmitter Enable */
#define E1000_TCTL_PSP   (1U << 3)   /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT  4       /* Collision Threshold */
#define E1000_TCTL_COLD_SHIFT 12     /* Collision Distance */

/* ICR / IMS bits */
#define E1000_ICR_TXDW   (1U << 0)   /* TX Descriptor Written Back */
#define E1000_ICR_TXQE   (1U << 1)   /* TX Queue Empty */
#define E1000_ICR_LSC    (1U << 2)   /* Link Status Change */
#define E1000_ICR_RXDMT0 (1U << 4)   /* RX Descriptor Minimum Threshold */
#define E1000_ICR_RXO    (1U << 6)   /* Receiver Overrun */
#define E1000_ICR_RXT0   (1U << 7)   /* Receiver Timer Interrupt */

/* EERD bits */
#define E1000_EERD_START (1U << 0)
#define E1000_EERD_DONE  (1U << 4)

/* TX command bits */
#define E1000_TXD_CMD_EOP  (1U << 0) /* End of Packet */
#define E1000_TXD_CMD_IFCS (1U << 1) /* Insert FCS/CRC */
#define E1000_TXD_CMD_RS   (1U << 3) /* Report Status */

/* TX status bits */
#define E1000_TXD_STAT_DD  (1U << 0) /* Descriptor Done */

/* RX status bits */
#define E1000_RXD_STAT_DD  (1U << 0) /* Descriptor Done */
#define E1000_RXD_STAT_EOP (1U << 1) /* End of Packet */

/* Ring sizes */
#define E1000_NUM_TX_DESC  32
#define E1000_NUM_RX_DESC  32
#define E1000_RX_BUF_SIZE  2048
#define E1000_TX_BUF_SIZE  2048

/* TX Descriptor (Legacy) */
struct e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

/* RX Descriptor */
struct e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

/* RX semaphore — signaled by IRQ handler, waited on by RX thread */
#include "sync.h"
extern ksem_t e1000_rx_sem;

/* Initialize the E1000 NIC. Returns 0 on success, -1 on failure. */
int e1000_init(void);

/* Send a packet. Returns 0 on success. */
int e1000_send(const void* data, uint16_t len);

/* Receive a packet into buf (max buf_len bytes). Returns bytes received, or 0. */
int e1000_recv(void* buf, uint16_t buf_len);

/* Get the MAC address (6 bytes). */
void e1000_get_mac(uint8_t mac[6]);

/* Check if the NIC is initialized and link is up. */
int e1000_link_up(void);

void e1000_driver_register(void);

#endif
