#include "e1000.h"
#include "pci.h"
#include "vmm.h"
#include "pmm.h"
#include "interrupts.h"
#include "uart_console.h"
#include "utils.h"
#include "io.h"

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Kernel VA layout for E1000 DMA buffers                             */
/*   0xC0230000 .. 0xC024FFFF  E1000 MMIO (128 KB = 32 pages)        */
/*   0xC0250000                TX descriptor ring (1 page)            */
/*   0xC0251000                RX descriptor ring (1 page)            */
/*   0xC0252000 .. 0xC0261FFF  TX buffers (32 x 2 KB = 16 pages)     */
/*   0xC0262000 .. 0xC0271FFF  RX buffers (32 x 2 KB = 16 pages)     */
/* ------------------------------------------------------------------ */
#define E1000_MMIO_VA      0xC0230000U
#define E1000_MMIO_PAGES   32
#define E1000_TX_DESC_VA   0xC0250000U
#define E1000_RX_DESC_VA   0xC0251000U
#define E1000_TX_BUF_VA    0xC0252000U
#define E1000_RX_BUF_VA    0xC0262000U

static volatile uint32_t* e1000_mmio = 0;
static uint8_t e1000_mac[6];
static int e1000_ready = 0;

/* Physical addresses for DMA */
static uint32_t tx_desc_phys;
static uint32_t rx_desc_phys;
static uint32_t tx_buf_phys[E1000_NUM_TX_DESC];
static uint32_t rx_buf_phys[E1000_NUM_RX_DESC];

/* Ring indices */
static volatile uint32_t tx_tail = 0;
static volatile uint32_t rx_tail = 0;

/* Cached PCI device info */
static uint8_t e1000_bus, e1000_slot, e1000_func;

/* ------------------------------------------------------------------ */
/* MMIO helpers                                                       */
/* ------------------------------------------------------------------ */

static inline uint32_t e1000_read(uint32_t reg) {
    return e1000_mmio[reg / 4];
}

static inline void e1000_write(uint32_t reg, uint32_t val) {
    e1000_mmio[reg / 4] = val;
}

/* ------------------------------------------------------------------ */
/* EEPROM                                                             */
/* ------------------------------------------------------------------ */

static uint16_t e1000_eeprom_read(uint8_t addr) {
    e1000_write(E1000_EERD, ((uint32_t)addr << 8) | E1000_EERD_START);
    uint32_t val;
    for (int i = 0; i < 1000; i++) {
        val = e1000_read(E1000_EERD);
        if (val & E1000_EERD_DONE)
            return (uint16_t)(val >> 16);
    }
    return 0;
}

static void e1000_read_mac(void) {
    uint16_t w0 = e1000_eeprom_read(0);
    uint16_t w1 = e1000_eeprom_read(1);
    uint16_t w2 = e1000_eeprom_read(2);
    e1000_mac[0] = (uint8_t)(w0 & 0xFF);
    e1000_mac[1] = (uint8_t)(w0 >> 8);
    e1000_mac[2] = (uint8_t)(w1 & 0xFF);
    e1000_mac[3] = (uint8_t)(w1 >> 8);
    e1000_mac[4] = (uint8_t)(w2 & 0xFF);
    e1000_mac[5] = (uint8_t)(w2 >> 8);
}

/* ------------------------------------------------------------------ */
/* DMA memory allocation                                              */
/* ------------------------------------------------------------------ */

static uint32_t alloc_dma_page(uintptr_t va) {
    void* phys = pmm_alloc_page();
    if (!phys) return 0;
    vmm_map_page((uint64_t)(uintptr_t)phys, (uint64_t)va,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_NOCACHE);
    memset((void*)va, 0, 4096);
    return (uint32_t)(uintptr_t)phys;
}

/* ------------------------------------------------------------------ */
/* TX ring setup                                                      */
/* ------------------------------------------------------------------ */

static int e1000_init_tx(void) {
    tx_desc_phys = alloc_dma_page(E1000_TX_DESC_VA);
    if (!tx_desc_phys) return -1;

    struct e1000_tx_desc* txd = (struct e1000_tx_desc*)E1000_TX_DESC_VA;

    /* Allocate TX buffers: 2 buffers per page (2048 each) */
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        int page_idx = i / 2;
        int buf_off  = (i % 2) * E1000_TX_BUF_SIZE;
        uintptr_t va = E1000_TX_BUF_VA + (uintptr_t)page_idx * 4096;

        if (buf_off == 0) {
            /* First buffer on this page — allocate it */
            uint32_t phys = alloc_dma_page(va);
            if (!phys) return -1;
            tx_buf_phys[i] = phys;
        } else {
            tx_buf_phys[i] = tx_buf_phys[i - 1] + E1000_TX_BUF_SIZE;
        }

        txd[i].buffer_addr = (uint64_t)tx_buf_phys[i];
        txd[i].cmd = 0;
        txd[i].status = E1000_TXD_STAT_DD; /* Mark as done so first send works */
    }

    e1000_write(E1000_TDBAL, tx_desc_phys);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * (uint32_t)sizeof(struct e1000_tx_desc));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    tx_tail = 0;

    /* Enable transmitter */
    e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                (15U << E1000_TCTL_CT_SHIFT) |
                (64U << E1000_TCTL_COLD_SHIFT));

    /* Inter-packet gap: recommended 10, 8, 6 for copper */
    e1000_write(E1000_TIPG, 10 | (8 << 10) | (6 << 20));

    return 0;
}

/* ------------------------------------------------------------------ */
/* RX ring setup                                                      */
/* ------------------------------------------------------------------ */

static int e1000_init_rx(void) {
    rx_desc_phys = alloc_dma_page(E1000_RX_DESC_VA);
    if (!rx_desc_phys) return -1;

    struct e1000_rx_desc* rxd = (struct e1000_rx_desc*)E1000_RX_DESC_VA;

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        int page_idx = i / 2;
        int buf_off  = (i % 2) * E1000_RX_BUF_SIZE;
        uintptr_t va = E1000_RX_BUF_VA + (uintptr_t)page_idx * 4096;

        if (buf_off == 0) {
            uint32_t phys = alloc_dma_page(va);
            if (!phys) return -1;
            rx_buf_phys[i] = phys;
        } else {
            rx_buf_phys[i] = rx_buf_phys[i - 1] + E1000_RX_BUF_SIZE;
        }

        rxd[i].buffer_addr = (uint64_t)rx_buf_phys[i];
        rxd[i].status = 0;
    }

    /* Set receive address (MAC filter) */
    uint32_t ral = (uint32_t)e1000_mac[0] | ((uint32_t)e1000_mac[1] << 8) |
                   ((uint32_t)e1000_mac[2] << 16) | ((uint32_t)e1000_mac[3] << 24);
    uint32_t rah = (uint32_t)e1000_mac[4] | ((uint32_t)e1000_mac[5] << 8) |
                   (1U << 31); /* Address Valid */
    e1000_write(E1000_RAL0, ral);
    e1000_write(E1000_RAH0, rah);

    /* Clear multicast table */
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + (uint32_t)i * 4, 0);
    }

    e1000_write(E1000_RDBAL, rx_desc_phys);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * (uint32_t)sizeof(struct e1000_rx_desc));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);
    rx_tail = 0;

    /* Enable receiver: accept broadcast, 2048-byte buffers, strip CRC */
    e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM |
                E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Interrupt handler                                                  */
/* ------------------------------------------------------------------ */

static void e1000_irq_handler(struct registers* regs) {
    (void)regs;
    uint32_t icr = e1000_read(E1000_ICR);
    (void)icr;
    /* Reading ICR clears the pending interrupt bits.
     * RX/TX processing is done via polling in e1000_recv/e1000_send
     * for simplicity. The interrupt just wakes the system. */
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int e1000_init(void) {
    const struct pci_device* dev = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID);
    if (!dev) {
        uart_print("[E1000] Device not found.\n");
        return -1;
    }

    e1000_bus  = dev->bus;
    e1000_slot = dev->slot;
    e1000_func = dev->func;

    /* Read BAR0 (MMIO base) */
    uint32_t bar0 = dev->bar[0];
    if (bar0 & 1) {
        uart_print("[E1000] BAR0 is I/O (unsupported).\n");
        return -1;
    }
    uint32_t mmio_phys = bar0 & 0xFFFFFFF0U;

    /* Map E1000 MMIO region (128KB) */
    for (int i = 0; i < E1000_MMIO_PAGES; i++) {
        vmm_map_page((uint64_t)(mmio_phys + (uint32_t)i * 4096),
                     (uint64_t)(E1000_MMIO_VA + (uint32_t)i * 4096),
                     VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_NOCACHE);
    }
    e1000_mmio = (volatile uint32_t*)E1000_MMIO_VA;

    /* Enable PCI bus mastering + memory space */
    uint32_t cmd = pci_config_read(e1000_bus, e1000_slot, e1000_func, 0x04);
    cmd |= (1U << 2) | (1U << 1); /* Bus Master | Memory Space */
    pci_config_write(e1000_bus, e1000_slot, e1000_func, 0x04, cmd);

    /* Reset the device */
    uint32_t ctrl = e1000_read(E1000_CTRL);
    e1000_write(E1000_CTRL, ctrl | E1000_CTRL_RST);
    /* Wait for reset to complete (spec says ~1us, be generous) */
    for (volatile int i = 0; i < 100000; i++) { }

    /* Disable interrupts during setup */
    e1000_write(E1000_IMC, 0xFFFFFFFF);
    e1000_read(E1000_ICR);  /* Clear pending */

    /* Set link up */
    ctrl = e1000_read(E1000_CTRL);
    e1000_write(E1000_CTRL, ctrl | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    /* Read MAC address from EEPROM */
    e1000_read_mac();

    uart_print("[E1000] MAC: ");
    char hex[4];
    for (int i = 0; i < 6; i++) {
        if (i) uart_print(":");
        hex[0] = "0123456789ABCDEF"[(e1000_mac[i] >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[e1000_mac[i] & 0xF];
        hex[2] = '\0';
        uart_print(hex);
    }
    uart_print("\n");

    /* Init TX and RX rings */
    if (e1000_init_tx() < 0) {
        uart_print("[E1000] Failed to init TX ring.\n");
        return -1;
    }
    if (e1000_init_rx() < 0) {
        uart_print("[E1000] Failed to init RX ring.\n");
        return -1;
    }

    /* Register interrupt handler */
    uint8_t irq = dev->irq_line;
    if (irq < 16) {
        register_interrupt_handler((uint8_t)(32 + irq), e1000_irq_handler);
    }

    /* Enable RX interrupts */
    e1000_write(E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_LSC |
                E1000_ICR_RXDMT0 | E1000_ICR_RXO);

    e1000_ready = 1;

    char buf[12];
    uart_print("[E1000] Initialized, IRQ=");
    itoa(irq, buf, 10);
    uart_print(buf);
    uart_print(", MMIO=");
    itoa_hex(mmio_phys, buf);
    uart_print(buf);
    uart_print("\n");

    return 0;
}

int e1000_send(const void* data, uint16_t len) {
    if (!e1000_ready || !data || len == 0 || len > E1000_TX_BUF_SIZE)
        return -1;

    struct e1000_tx_desc* txd = (struct e1000_tx_desc*)E1000_TX_DESC_VA;
    uint32_t idx = tx_tail;

    /* Wait for descriptor to be available */
    int timeout = 100000;
    while (!(txd[idx].status & E1000_TXD_STAT_DD) && --timeout > 0) { }
    if (timeout <= 0) return -1;

    /* Copy data to TX buffer */
    uintptr_t buf_va = E1000_TX_BUF_VA + (uintptr_t)(idx / 2) * 4096 +
                       (uintptr_t)(idx % 2) * E1000_TX_BUF_SIZE;
    memcpy((void*)buf_va, data, len);

    /* Set up descriptor */
    txd[idx].buffer_addr = (uint64_t)tx_buf_phys[idx];
    txd[idx].length = len;
    txd[idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    txd[idx].status = 0;

    /* Advance tail */
    tx_tail = (idx + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_TDT, tx_tail);

    return 0;
}

int e1000_recv(void* buf, uint16_t buf_len) {
    if (!e1000_ready || !buf || buf_len == 0)
        return 0;

    struct e1000_rx_desc* rxd = (struct e1000_rx_desc*)E1000_RX_DESC_VA;
    uint32_t idx = rx_tail;

    if (!(rxd[idx].status & E1000_RXD_STAT_DD))
        return 0;  /* No packet available */

    uint16_t pkt_len = rxd[idx].length;
    if (pkt_len > buf_len) pkt_len = buf_len;

    /* Copy data from RX buffer */
    uintptr_t buf_va = E1000_RX_BUF_VA + (uintptr_t)(idx / 2) * 4096 +
                       (uintptr_t)(idx % 2) * E1000_RX_BUF_SIZE;
    memcpy(buf, (const void*)buf_va, pkt_len);

    /* Reset descriptor and advance tail */
    rxd[idx].status = 0;
    uint32_t old_tail = rx_tail;
    rx_tail = (idx + 1) % E1000_NUM_RX_DESC;
    e1000_write(E1000_RDT, old_tail);

    return (int)pkt_len;
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = e1000_mac[i];
}

int e1000_link_up(void) {
    if (!e1000_ready) return 0;
    return (e1000_read(E1000_STATUS) & (1U << 1)) ? 1 : 0;
}
