#include "ata_dma.h"
#include "kernel_va_map.h"
#include "pci.h"
#include "io.h"
#include "pmm.h"
#include "vmm.h"
#include "arch/x86/idt.h"
#include "spinlock.h"
#include "console.h"
#include "utils.h"
#include "errno.h"

#include <stdint.h>
#include <stddef.h>

/* ATA register offsets */
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07

#define ATA_CMD_READ_DMA   0xC8
#define ATA_CMD_WRITE_DMA  0xCA
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_BSY   0x80
#define ATA_SR_DRDY  0x40
#define ATA_SR_DF    0x20
#define ATA_SR_DRQ   0x08
#define ATA_SR_ERR   0x01

/* Bus Master IDE register offsets (from channel BM base) */
#define BM_CMD      0x00
#define BM_STATUS   0x02
#define BM_PRDT     0x04

#define BM_CMD_START  0x01
#define BM_CMD_READ   0x08

#define BM_STATUS_ACTIVE  0x01
#define BM_STATUS_ERR     0x02
#define BM_STATUS_IRQ     0x04

/* Physical Region Descriptor (PRD) entry */
struct prd_entry {
    uint32_t phys_addr;
    uint16_t byte_count;
    uint16_t flags;
} __attribute__((packed));

/* Per-channel I/O bases */
static const uint16_t ch_io[ATA_NUM_CHANNELS]   = { 0x1F0, 0x170 };
static const uint16_t ch_ctrl[ATA_NUM_CHANNELS]  = { 0x3F6, 0x376 };
static const uint8_t  ch_irq[ATA_NUM_CHANNELS]   = { 46, 47 };

/* Per-channel KVA addresses for PRDT and bounce buffer */
static const uint32_t ch_kva_prdt[ATA_NUM_CHANNELS] = {
    KVA_ATA_DMA_PRDT_PRI, KVA_ATA_DMA_PRDT_SEC
};
static const uint32_t ch_kva_buf[ATA_NUM_CHANNELS] = {
    KVA_ATA_DMA_BUF_PRI, KVA_ATA_DMA_BUF_SEC
};

/* Per-channel DMA state */
struct dma_ch_state {
    int available;
    uint16_t bm_base;
    struct prd_entry* prdt;
    uint32_t prdt_phys;
    uint8_t* dma_buf;
    uint32_t dma_buf_phys;
    volatile int dma_active;
    spinlock_t lock;
};

static struct dma_ch_state dma_ch[ATA_NUM_CHANNELS];

static inline void io_wait_400ns_ch(int channel) {
    (void)inb(ch_ctrl[channel]);
    (void)inb(ch_ctrl[channel]);
    (void)inb(ch_ctrl[channel]);
    (void)inb(ch_ctrl[channel]);
}

static int ata_wait_not_busy_ch(int channel) {
    uint16_t io = ch_io[channel];
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
        if ((st & ATA_SR_BSY) == 0) return 0;
    }
    return -EIO;
}

/* IRQ handlers — clear BM IRQ bit during DMA, or just read ATA status */
static void ata_dma_irq14(struct registers* regs) {
    (void)regs;
    struct dma_ch_state* s = &dma_ch[ATA_CHANNEL_PRIMARY];
    if (s->dma_active && s->bm_base) {
        uint8_t bm_stat = inb((uint16_t)(s->bm_base + BM_STATUS));
        outb((uint16_t)(s->bm_base + BM_STATUS), bm_stat | BM_STATUS_IRQ);
    } else {
        (void)inb((uint16_t)(ch_io[0] + ATA_REG_STATUS));
    }
}

static void ata_dma_irq15(struct registers* regs) {
    (void)regs;
    struct dma_ch_state* s = &dma_ch[ATA_CHANNEL_SECONDARY];
    if (s->dma_active && s->bm_base) {
        uint8_t bm_stat = inb((uint16_t)(s->bm_base + BM_STATUS));
        outb((uint16_t)(s->bm_base + BM_STATUS), bm_stat | BM_STATUS_IRQ);
    } else {
        (void)inb((uint16_t)(ch_io[1] + ATA_REG_STATUS));
    }
}

/* PCI Bus Master base — shared between both channels, offset 0x08 for secondary */
static uint16_t pci_bm_base = 0;
static int pci_bm_probed = 0;

static int ata_dma_probe_pci(void) {
    if (pci_bm_probed) return pci_bm_base ? 0 : -ENODEV;
    pci_bm_probed = 1;

    const struct pci_device* ide = pci_find_class(0x01, 0x01);
    if (!ide) return -ENODEV;

    uint32_t bar4 = ide->bar[4];
    if ((bar4 & 1) == 0) return -ENODEV;

    pci_bm_base = (uint16_t)(bar4 & 0xFFFC);
    if (pci_bm_base == 0) return -ENODEV;

    /* Enable PCI Bus Mastering + I/O space */
    uint32_t cmd_reg = pci_config_read(ide->bus, ide->slot, ide->func, 0x04);
    cmd_reg |= (1U << 0) | (1U << 2);
    pci_config_write(ide->bus, ide->slot, ide->func, 0x04, cmd_reg);

    return 0;
}

int ata_dma_init(int channel) {
    if (channel < 0 || channel >= ATA_NUM_CHANNELS) return -EINVAL;
    struct dma_ch_state* s = &dma_ch[channel];
    if (s->available) return 0;

    if (ata_dma_probe_pci() < 0) return -ENODEV;

    /* Primary at BAR4+0, secondary at BAR4+8 */
    s->bm_base = pci_bm_base + (uint16_t)(channel * 8);

    /* Allocate PRDT page */
    void* prdt_page = pmm_alloc_page();
    if (!prdt_page) return -ENOMEM;

    s->prdt_phys = (uint32_t)(uintptr_t)prdt_page;
    uintptr_t prdt_virt = ch_kva_prdt[channel];
    vmm_map_page((uint64_t)s->prdt_phys, (uint64_t)prdt_virt,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);
    s->prdt = (struct prd_entry*)prdt_virt;
    memset((void*)s->prdt, 0, PAGE_SIZE);

    /* Allocate bounce buffer page */
    void* buf_page = pmm_alloc_page();
    if (!buf_page) {
        pmm_free_page(prdt_page);
        return -ENOMEM;
    }

    s->dma_buf_phys = (uint32_t)(uintptr_t)buf_page;
    uintptr_t buf_virt = ch_kva_buf[channel];
    vmm_map_page((uint64_t)s->dma_buf_phys, (uint64_t)buf_virt,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);
    s->dma_buf = (uint8_t*)buf_virt;

    /* Set up single PRD entry: 512 bytes, EOT */
    s->prdt[0].phys_addr = s->dma_buf_phys;
    s->prdt[0].byte_count = 512;
    s->prdt[0].flags = 0x8000;

    /* Register DMA-aware IRQ handler for this channel */
    if (channel == 0) {
        register_interrupt_handler(ch_irq[0], ata_dma_irq14);
    } else {
        register_interrupt_handler(ch_irq[1], ata_dma_irq15);
    }

    /* Stop any in-progress DMA and clear status */
    outb((uint16_t)(s->bm_base + BM_CMD), 0);
    outb((uint16_t)(s->bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);

    s->available = 1;
    kprintf("[ATA-DMA] Ch%d initialized, BM I/O base=0x%x\n",
            channel, (unsigned)s->bm_base);

    return 0;
}

int ata_dma_available(int channel) {
    if (channel < 0 || channel >= ATA_NUM_CHANNELS) return 0;
    return dma_ch[channel].available;
}

/* Common per-channel DMA transfer */
static int ata_dma_transfer(int channel, int slave, uint32_t lba, int is_write) {
    if (lba & 0xF0000000U) return -EINVAL;
    struct dma_ch_state* s = &dma_ch[channel];
    uint16_t io   = ch_io[channel];
    uint16_t ctrl = ch_ctrl[channel];

    /* Clear nIEN so device asserts INTRQ */
    outb(ctrl, 0x00);

    /* Read ATA status to clear any pending interrupt */
    (void)inb((uint16_t)(io + ATA_REG_STATUS));

    /* Set PRDT address */
    outl((uint16_t)(s->bm_base + BM_PRDT), s->prdt_phys);

    /* Clear status bits */
    outb((uint16_t)(s->bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);

    /* Wait for drive not busy */
    if (ata_wait_not_busy_ch(channel) < 0) return -EIO;

    /* Select drive + LBA */
    uint8_t sel = slave ? 0xF0 : 0xE0;
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(sel | ((lba >> 24) & 0x0F)));
    io_wait_400ns_ch(channel);

    /* Sector count and LBA */
    outb((uint16_t)(io + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));

    __atomic_store_n(&s->dma_active, 1, __ATOMIC_SEQ_CST);

    /* Set direction bit (without Start) */
    uint8_t bm_dir = is_write ? 0x00 : BM_CMD_READ;
    outb((uint16_t)(s->bm_base + BM_CMD), bm_dir);

    /* Issue ATA DMA command */
    outb((uint16_t)(io + ATA_REG_COMMAND),
         is_write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA);

    /* Start DMA */
    outb((uint16_t)(s->bm_base + BM_CMD), bm_dir | BM_CMD_START);

    /* Poll for completion */
    int completed = 0;
    for (int i = 0; i < 2000000; i++) {
        uint8_t ata_stat = inb((uint16_t)(io + ATA_REG_STATUS));
        uint8_t bm_stat  = inb((uint16_t)(s->bm_base + BM_STATUS));

        if (bm_stat & BM_STATUS_ERR) {
            outb((uint16_t)(s->bm_base + BM_CMD), 0);
            outb((uint16_t)(s->bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);
            __atomic_store_n(&s->dma_active, 0, __ATOMIC_SEQ_CST);
            return -EIO;
        }
        if (ata_stat & ATA_SR_ERR) {
            outb((uint16_t)(s->bm_base + BM_CMD), 0);
            outb((uint16_t)(s->bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);
            __atomic_store_n(&s->dma_active, 0, __ATOMIC_SEQ_CST);
            return -EIO;
        }
        if (!(ata_stat & ATA_SR_BSY) && !(bm_stat & BM_STATUS_ACTIVE)) {
            completed = 1;
            break;
        }
    }

    outb((uint16_t)(s->bm_base + BM_CMD), 0);
    outb((uint16_t)(s->bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);
    __atomic_store_n(&s->dma_active, 0, __ATOMIC_SEQ_CST);

    return completed ? 0 : -EIO;
}

int ata_dma_read28(int channel, int slave, uint32_t lba, uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (channel < 0 || channel >= ATA_NUM_CHANNELS) return -EINVAL;
    struct dma_ch_state* s = &dma_ch[channel];
    if (!s->available) return -ENOSYS;

    spin_lock(&s->lock);
    int ret = ata_dma_transfer(channel, slave, lba, 0);
    if (ret == 0) memcpy(buf512, s->dma_buf, 512);
    spin_unlock(&s->lock);
    return ret;
}

int ata_dma_write28(int channel, int slave, uint32_t lba, const uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (channel < 0 || channel >= ATA_NUM_CHANNELS) return -EINVAL;
    struct dma_ch_state* s = &dma_ch[channel];
    if (!s->available) return -ENOSYS;

    spin_lock(&s->lock);
    memcpy(s->dma_buf, buf512, 512);
    int ret = ata_dma_transfer(channel, slave, lba, 1);
    if (ret == 0) {
        outb((uint16_t)(ch_io[channel] + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
        (void)ata_wait_not_busy_ch(channel);
    }
    spin_unlock(&s->lock);
    return ret;
}

int ata_dma_read_direct(int channel, int slave, uint32_t lba,
                        uint32_t phys_buf, uint16_t byte_count) {
    if (channel < 0 || channel >= ATA_NUM_CHANNELS) return -EINVAL;
    struct dma_ch_state* s = &dma_ch[channel];
    if (!s->available) return -ENOSYS;
    if (phys_buf == 0 || (phys_buf & 1)) return -EINVAL;
    if (byte_count == 0) byte_count = 512;

    spin_lock(&s->lock);

    uint32_t saved_addr  = s->prdt[0].phys_addr;
    uint16_t saved_count = s->prdt[0].byte_count;
    s->prdt[0].phys_addr  = phys_buf;
    s->prdt[0].byte_count = byte_count;

    int ret = ata_dma_transfer(channel, slave, lba, 0);

    s->prdt[0].phys_addr  = saved_addr;
    s->prdt[0].byte_count = saved_count;

    spin_unlock(&s->lock);
    return ret;
}

int ata_dma_write_direct(int channel, int slave, uint32_t lba,
                         uint32_t phys_buf, uint16_t byte_count) {
    if (channel < 0 || channel >= ATA_NUM_CHANNELS) return -EINVAL;
    struct dma_ch_state* s = &dma_ch[channel];
    if (!s->available) return -ENOSYS;
    if (phys_buf == 0 || (phys_buf & 1)) return -EINVAL;
    if (byte_count == 0) byte_count = 512;

    spin_lock(&s->lock);

    uint32_t saved_addr  = s->prdt[0].phys_addr;
    uint16_t saved_count = s->prdt[0].byte_count;
    s->prdt[0].phys_addr  = phys_buf;
    s->prdt[0].byte_count = byte_count;

    int ret = ata_dma_transfer(channel, slave, lba, 1);

    if (ret == 0) {
        outb((uint16_t)(ch_io[channel] + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
        (void)ata_wait_not_busy_ch(channel);
    }

    s->prdt[0].phys_addr  = saved_addr;
    s->prdt[0].byte_count = saved_count;

    spin_unlock(&s->lock);
    return ret;
}
