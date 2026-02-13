#include "ata_pio.h"
#include "ata_dma.h"

#include "errno.h"
#include "io.h"
#include "arch/x86/idt.h"
#include "console.h"

/* Basic IRQ 14 handler: read ATA status to deassert INTRQ.
 * Registered early so PIO IDENTIFY doesn't cause an IRQ storm. */
static void ata_pio_irq14_handler(struct registers* regs) {
    (void)regs;
    (void)inb((uint16_t)(0x1F0 + 0x07));  /* Read ATA status */
}

// Primary ATA bus I/O ports
#define ATA_IO_BASE 0x1F0
#define ATA_CTRL_BASE 0x3F6

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH   0xE7
#define ATA_CMD_IDENTIFY      0xEC

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_DSC  0x10
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static inline void io_wait_400ns(void) {
    (void)inb((uint16_t)ATA_CTRL_BASE);
    (void)inb((uint16_t)ATA_CTRL_BASE);
    (void)inb((uint16_t)ATA_CTRL_BASE);
    (void)inb((uint16_t)ATA_CTRL_BASE);
}

static int ata_wait_not_busy(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb((uint16_t)(ATA_IO_BASE + ATA_REG_STATUS));
        if ((st & ATA_SR_BSY) == 0) return 0;
    }
    return -EIO;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb((uint16_t)(ATA_IO_BASE + ATA_REG_STATUS));
        if (st & ATA_SR_ERR) return -EIO;
        if (st & ATA_SR_DF) return -EIO;
        if ((st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ)) return 0;
    }
    return -EIO;
}

uint32_t ata_pio_sector_size(void) {
    return 512;
}

static int ata_pio_inited = 0;

int ata_pio_init_primary_master(void) {
    if (ata_pio_inited) return 0;

    // Register IRQ 14 handler early to prevent INTRQ storm
    register_interrupt_handler(46, ata_pio_irq14_handler);

    // Select drive: 0xA0 = master, CHS mode bits set, LBA bit cleared
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_HDDEVSEL), 0xA0);
    io_wait_400ns();

    if (ata_wait_not_busy() < 0) return -EIO;

    // IDENTIFY
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_SECCOUNT0), 0);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA0), 0);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA1), 0);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA2), 0);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);

    uint8_t st = inb((uint16_t)(ATA_IO_BASE + ATA_REG_STATUS));
    if (st == 0) return -ENODEV;

    if (ata_wait_drq() < 0) return -EIO;

    // Read 256 words (512 bytes) identify data and discard.
    for (int i = 0; i < 256; i++) {
        (void)inw((uint16_t)(ATA_IO_BASE + ATA_REG_DATA));
    }

    // Try to upgrade to DMA mode
    if (ata_dma_init() == 0) {
        kprintf("[ATA] Using DMA mode.\n");
    } else {
        kprintf("[ATA] Using PIO mode (DMA unavailable).\n");
    }

    ata_pio_inited = 1;
    return 0;
}

int ata_pio_read28(uint32_t lba, uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (lba & 0xF0000000U) return -EINVAL;

    // Use DMA if available
    if (ata_dma_available()) {
        return ata_dma_read28(lba, buf512);
    }

    if (ata_wait_not_busy() < 0) return -EIO;

    outb((uint16_t)(ATA_IO_BASE + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait_400ns();

    outb((uint16_t)(ATA_IO_BASE + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND), ATA_CMD_READ_SECTORS);

    if (ata_wait_drq() < 0) return -EIO;

    uint16_t* w = (uint16_t*)buf512;
    for (int i = 0; i < 256; i++) {
        w[i] = inw((uint16_t)(ATA_IO_BASE + ATA_REG_DATA));
    }

    io_wait_400ns();
    return 0;
}

int ata_pio_write28(uint32_t lba, const uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (lba & 0xF0000000U) return -EINVAL;

    // Use DMA if available
    if (ata_dma_available()) {
        return ata_dma_write28(lba, buf512);
    }

    if (ata_wait_not_busy() < 0) return -EIO;

    outb((uint16_t)(ATA_IO_BASE + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait_400ns();

    outb((uint16_t)(ATA_IO_BASE + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND), ATA_CMD_WRITE_SECTORS);

    if (ata_wait_drq() < 0) return -EIO;

    const uint16_t* w = (const uint16_t*)buf512;
    for (int i = 0; i < 256; i++) {
        outw((uint16_t)(ATA_IO_BASE + ATA_REG_DATA), w[i]);
    }

    outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
    (void)ata_wait_not_busy();
    io_wait_400ns();
    return 0;
}
