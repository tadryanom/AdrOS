#include "ata_pio.h"
#include "ata_dma.h"

#include "errno.h"
#include "io.h"
#include "arch/x86/idt.h"
#include "console.h"
#include "utils.h"

/* ATA register offsets */
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

/* Channel I/O port bases */
static const uint16_t ch_io_base[ATA_NUM_CHANNELS]   = { 0x1F0, 0x170 };
static const uint16_t ch_ctrl_base[ATA_NUM_CHANNELS]  = { 0x3F6, 0x376 };
static const uint8_t  ch_irq_vec[ATA_NUM_CHANNELS]    = { 46, 47 };

/* Drive presence flags */
static int drive_present[ATA_MAX_DRIVES];
static int ata_pio_inited = 0;

static const char* drive_names[ATA_MAX_DRIVES] = { "hda", "hdb", "hdc", "hdd" };

/* ---- Low-level helpers ---- */

static inline void io_wait_400ns(uint16_t ctrl) {
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
}

static int ata_wait_not_busy(uint16_t io) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
        if ((st & ATA_SR_BSY) == 0) return 0;
    }
    return -EIO;
}

static int ata_wait_drq(uint16_t io) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
        if (st & ATA_SR_ERR) return -EIO;
        if (st & ATA_SR_DF)  return -EIO;
        if ((st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ)) return 0;
    }
    return -EIO;
}

/* ---- IRQ handlers (deassert INTRQ by reading status) ---- */

static void ata_irq14_handler(struct registers* regs) {
    (void)regs;
    (void)inb((uint16_t)(0x1F0 + ATA_REG_STATUS));
}

static void ata_irq15_handler(struct registers* regs) {
    (void)regs;
    (void)inb((uint16_t)(0x170 + ATA_REG_STATUS));
}

/* ---- Drive probing ---- */

static int ata_probe_drive(int channel, int slave) {
    uint16_t io   = ch_io_base[channel];
    uint16_t ctrl = ch_ctrl_base[channel];

    /* Select drive */
    uint8_t sel = slave ? 0xB0 : 0xA0;
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), sel);
    io_wait_400ns(ctrl);

    if (ata_wait_not_busy(io) < 0) return 0;

    /* Zero registers */
    outb((uint16_t)(io + ATA_REG_SECCOUNT0), 0);
    outb((uint16_t)(io + ATA_REG_LBA0), 0);
    outb((uint16_t)(io + ATA_REG_LBA1), 0);
    outb((uint16_t)(io + ATA_REG_LBA2), 0);

    /* IDENTIFY */
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);

    uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
    if (st == 0) return 0; /* No drive */

    /* Wait for BSY to clear */
    for (int i = 0; i < 1000000; i++) {
        st = inb((uint16_t)(io + ATA_REG_STATUS));
        if (!(st & ATA_SR_BSY)) break;
    }
    if (st & ATA_SR_BSY) return 0;

    /* Non-zero LBA1/LBA2 means ATAPI — skip */
    uint8_t lba1 = inb((uint16_t)(io + ATA_REG_LBA1));
    uint8_t lba2 = inb((uint16_t)(io + ATA_REG_LBA2));
    if (lba1 != 0 || lba2 != 0) return 0;

    /* Wait for DRQ */
    if (ata_wait_drq(io) < 0) return 0;

    /* Read and discard 256 words of identify data */
    for (int i = 0; i < 256; i++) {
        (void)inw((uint16_t)(io + ATA_REG_DATA));
    }

    return 1;
}

/* ---- Public API ---- */

uint32_t ata_pio_sector_size(void) {
    return 512;
}

int ata_pio_init(void) {
    if (ata_pio_inited) return 0;

    /* Register IRQ handlers for both channels */
    register_interrupt_handler(ch_irq_vec[0], ata_irq14_handler);
    register_interrupt_handler(ch_irq_vec[1], ata_irq15_handler);

    int found = 0;

    for (int ch = 0; ch < ATA_NUM_CHANNELS; ch++) {
        /* Floating bus check — 0xFF means no controller */
        uint8_t st = inb((uint16_t)(ch_io_base[ch] + ATA_REG_STATUS));
        if (st == 0xFF) {
            drive_present[ch * 2]     = 0;
            drive_present[ch * 2 + 1] = 0;
            continue;
        }

        for (int sl = 0; sl < 2; sl++) {
            int id = ch * 2 + sl;
            drive_present[id] = ata_probe_drive(ch, sl);
            if (drive_present[id]) found++;
        }

        /* Try DMA for this channel */
        if (ata_dma_init(ch) == 0) {
            kprintf("[ATA] Channel %d: DMA mode.\n", ch);
        } else {
            kprintf("[ATA] Channel %d: PIO mode.\n", ch);
        }
    }

    /* Log detected drives */
    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        if (drive_present[i]) {
            kprintf("[ATA] /dev/%s detected.\n", drive_names[i]);
        }
    }

    ata_pio_inited = 1;
    return found > 0 ? 0 : -ENODEV;
}

int ata_pio_drive_present(int drive) {
    if (drive < 0 || drive >= ATA_MAX_DRIVES) return 0;
    return drive_present[drive];
}

int ata_pio_read28(int drive, uint32_t lba, uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (drive < 0 || drive >= ATA_MAX_DRIVES) return -EINVAL;
    if (!drive_present[drive]) return -ENODEV;
    if (lba & 0xF0000000U) return -EINVAL;

    int ch = drive / 2;
    int sl = drive & 1;

    /* Use DMA if available for this channel */
    if (ata_dma_available(ch)) {
        return ata_dma_read28(ch, sl, lba, buf512);
    }

    uint16_t io   = ch_io_base[ch];
    uint16_t ctrl = ch_ctrl_base[ch];

    if (ata_wait_not_busy(io) < 0) return -EIO;

    uint8_t sel = sl ? 0xF0 : 0xE0;
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(sel | ((lba >> 24) & 0x0F)));
    io_wait_400ns(ctrl);

    outb((uint16_t)(io + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_READ_SECTORS);

    if (ata_wait_drq(io) < 0) return -EIO;

    uint16_t* w = (uint16_t*)buf512;
    for (int i = 0; i < 256; i++) {
        w[i] = inw((uint16_t)(io + ATA_REG_DATA));
    }

    io_wait_400ns(ctrl);
    return 0;
}

int ata_pio_write28(int drive, uint32_t lba, const uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (drive < 0 || drive >= ATA_MAX_DRIVES) return -EINVAL;
    if (!drive_present[drive]) return -ENODEV;
    if (lba & 0xF0000000U) return -EINVAL;

    int ch = drive / 2;
    int sl = drive & 1;

    if (ata_dma_available(ch)) {
        return ata_dma_write28(ch, sl, lba, buf512);
    }

    uint16_t io   = ch_io_base[ch];
    uint16_t ctrl = ch_ctrl_base[ch];

    if (ata_wait_not_busy(io) < 0) return -EIO;

    uint8_t sel = sl ? 0xF0 : 0xE0;
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(sel | ((lba >> 24) & 0x0F)));
    io_wait_400ns(ctrl);

    outb((uint16_t)(io + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_WRITE_SECTORS);

    if (ata_wait_drq(io) < 0) return -EIO;

    const uint16_t* w = (const uint16_t*)buf512;
    for (int i = 0; i < 256; i++) {
        outw((uint16_t)(io + ATA_REG_DATA), w[i]);
    }

    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
    (void)ata_wait_not_busy(io);
    io_wait_400ns(ctrl);
    return 0;
}

int ata_name_to_drive(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        if (strcmp(name, drive_names[i]) == 0) return i;
    }
    return -1;
}

const char* ata_drive_to_name(int drive) {
    if (drive < 0 || drive >= ATA_MAX_DRIVES) return 0;
    return drive_names[drive];
}
