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

/* ATA I/O ports (primary channel) */
#define ATA_IO_BASE     0x1F0
#define ATA_CTRL_BASE   0x3F6

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
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

/* Bus Master IDE register offsets (from BAR4) */
#define BM_CMD      0x00   /* Command register (byte) */
#define BM_STATUS   0x02   /* Status register (byte) */
#define BM_PRDT     0x04   /* PRDT address (dword) */

/* BM_CMD bits */
#define BM_CMD_START  0x01
#define BM_CMD_READ   0x08  /* 1 = device-to-memory (read), 0 = memory-to-device (write) */

/* BM_STATUS bits */
#define BM_STATUS_ACTIVE  0x01  /* Bus master active */
#define BM_STATUS_ERR     0x02  /* Error */
#define BM_STATUS_IRQ     0x04  /* Interrupt (write 1 to clear) */

/* Physical Region Descriptor (PRD) entry */
struct prd_entry {
    uint32_t phys_addr;   /* Physical buffer address */
    uint16_t byte_count;  /* Byte count (0 = 64KB) */
    uint16_t flags;       /* Bit 15 = end of table */
} __attribute__((packed));

/* State */
static int dma_available = 0;
static uint16_t bm_base = 0;           /* Bus Master I/O base port */
static struct prd_entry* prdt = NULL;   /* PRDT (kernel virtual) */
static uint32_t prdt_phys = 0;         /* PRDT physical address */
static uint8_t* dma_buf = NULL;        /* 512-byte DMA bounce buffer (kernel virtual) */
static uint32_t dma_buf_phys = 0;      /* DMA bounce buffer physical address */

static volatile int dma_active = 0;  /* Set during DMA polling to prevent IRQ handler race */
static spinlock_t dma_lock = {0};

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

/* IRQ 14 handler (vector 46): acknowledge device interrupt.
 * During active DMA polling, only clear BM status (polling loop handles ATA status).
 * During PIO operations, read ATA status to deassert INTRQ. */
static void ata_irq14_handler(struct registers* regs) {
    (void)regs;
    if (dma_active) {
        /* DMA polling loop is handling completion — just clear BM IRQ bit */
        uint8_t bm_stat = inb((uint16_t)(bm_base + BM_STATUS));
        outb((uint16_t)(bm_base + BM_STATUS), bm_stat | BM_STATUS_IRQ);
    } else {
        /* PIO mode — read ATA status to deassert INTRQ */
        (void)inb((uint16_t)(ATA_IO_BASE + ATA_REG_STATUS));
    }
}

int ata_dma_init(void) {
    if (dma_available) return 0;  /* Already initialized */

    /* Find IDE controller: PCI class 0x01 (Mass Storage), subclass 0x01 (IDE) */
    const struct pci_device* ide = pci_find_class(0x01, 0x01);
    if (!ide) {
        kprintf("[ATA-DMA] No PCI IDE controller found.\n");
        return -ENODEV;
    }

    /* BAR4 contains the Bus Master IDE I/O base */
    uint32_t bar4 = ide->bar[4];
    if ((bar4 & 1) == 0) {
        kprintf("[ATA-DMA] BAR4 is not I/O space.\n");
        return -ENODEV;
    }
    bm_base = (uint16_t)(bar4 & 0xFFFC);

    if (bm_base == 0) {
        kprintf("[ATA-DMA] BAR4 I/O base is zero.\n");
        return -ENODEV;
    }

    /* Enable PCI Bus Mastering (bit 2 of command register) + I/O space (bit 0) */
    uint32_t cmd_reg = pci_config_read(ide->bus, ide->slot, ide->func, 0x04);
    cmd_reg |= (1U << 0) | (1U << 2);  /* I/O Space Enable + Bus Master Enable */
    pci_config_write(ide->bus, ide->slot, ide->func, 0x04, cmd_reg);

    /* Allocate PRDT: one page, physically contiguous, aligned.
     * We only need 1 PRD entry (8 bytes) but allocate a full page. */
    void* prdt_page = pmm_alloc_page();
    if (!prdt_page) {
        kprintf("[ATA-DMA] Failed to allocate PRDT page.\n");
        return -ENOMEM;
    }
    prdt_phys = (uint32_t)(uintptr_t)prdt_page;
    /* Map PRDT at a dedicated VA — see include/kernel_va_map.h */
    uintptr_t prdt_virt = KVA_ATA_DMA_PRDT;
    vmm_map_page((uint64_t)prdt_phys, (uint64_t)prdt_virt,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);
    prdt = (struct prd_entry*)prdt_virt;
    memset((void*)prdt, 0, PAGE_SIZE);

    /* Allocate DMA bounce buffer: one page for sector transfers */
    void* buf_page = pmm_alloc_page();
    if (!buf_page) {
        kprintf("[ATA-DMA] Failed to allocate DMA buffer page.\n");
        pmm_free_page(prdt_page);
        return -ENOMEM;
    }
    dma_buf_phys = (uint32_t)(uintptr_t)buf_page;
    uintptr_t buf_virt = KVA_ATA_DMA_BUF;
    vmm_map_page((uint64_t)dma_buf_phys, (uint64_t)buf_virt,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);
    dma_buf = (uint8_t*)buf_virt;

    /* Set up the single PRD entry: 512 bytes, end-of-table */
    prdt[0].phys_addr = dma_buf_phys;
    prdt[0].byte_count = 512;
    prdt[0].flags = 0x8000;  /* EOT bit */

    /* Register IRQ 14 handler to acknowledge device interrupts */
    register_interrupt_handler(46, ata_irq14_handler);

    /* Stop any in-progress DMA and clear status */
    outb((uint16_t)(bm_base + BM_CMD), 0);
    outb((uint16_t)(bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);

    dma_available = 1;

    kprintf("[ATA-DMA] Initialized, BM I/O base=0x%x\n", (unsigned)bm_base);

    return 0;
}

int ata_dma_available(void) {
    return dma_available;
}

/* Common DMA transfer setup and wait */
static int ata_dma_transfer(uint32_t lba, int is_write) {
    if (lba & 0xF0000000U) return -EINVAL;

    /* Ensure nIEN is cleared so the device asserts INTRQ on completion.
     * The Bus Master IRQ bit won't be set without INTRQ. */
    outb((uint16_t)ATA_CTRL_BASE, 0x00);

    /* Read ATA status to clear any pending interrupt */
    (void)inb((uint16_t)(ATA_IO_BASE + ATA_REG_STATUS));

    /* Set PRDT address */
    outl((uint16_t)(bm_base + BM_PRDT), prdt_phys);

    /* Clear status bits */
    outb((uint16_t)(bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);

    /* Wait for drive not busy */
    if (ata_wait_not_busy() < 0) return -EIO;

    /* Select drive + LBA */
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_HDDEVSEL),
         (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait_400ns();

    /* Sector count and LBA */
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));

    /* Mark DMA as active so IRQ handler doesn't race on ATA status */
    __atomic_store_n(&dma_active, 1, __ATOMIC_SEQ_CST);

    /* Step 3: Set direction bit in BM Command (without Start bit yet) */
    uint8_t bm_dir = is_write ? 0x00 : BM_CMD_READ;
    outb((uint16_t)(bm_base + BM_CMD), bm_dir);

    /* Step 4: Issue ATA DMA command to the device */
    outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND),
         is_write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA);

    /* Step 5: Set Start bit to begin the DMA transfer */
    outb((uint16_t)(bm_base + BM_CMD), bm_dir | BM_CMD_START);

    /* Poll for DMA completion:
     * 1. Wait for ATA BSY to clear (device finished processing)
     * 2. Check BM status for Active bit clear (DMA engine done)
     * 3. Check for errors */
    int completed = 0;
    for (int i = 0; i < 2000000; i++) {
        uint8_t ata_stat = inb((uint16_t)(ATA_IO_BASE + ATA_REG_STATUS));
        uint8_t bm_stat = inb((uint16_t)(bm_base + BM_STATUS));

        /* Check for Bus Master error */
        if (bm_stat & BM_STATUS_ERR) {
            outb((uint16_t)(bm_base + BM_CMD), 0);
            outb((uint16_t)(bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);
            kprintf("[ATA-DMA] Bus master error!\n");
            return -EIO;
        }

        /* Check for ATA error */
        if (ata_stat & ATA_SR_ERR) {
            outb((uint16_t)(bm_base + BM_CMD), 0);
            outb((uint16_t)(bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);
            kprintf("[ATA-DMA] ATA error during DMA!\n");
            return -EIO;
        }

        /* DMA complete when: BSY clear AND BM Active clear */
        if (!(ata_stat & ATA_SR_BSY) && !(bm_stat & BM_STATUS_ACTIVE)) {
            completed = 1;
            break;
        }
    }

    /* Stop Bus Master */
    outb((uint16_t)(bm_base + BM_CMD), 0);

    /* Clear status bits */
    outb((uint16_t)(bm_base + BM_STATUS), BM_STATUS_IRQ | BM_STATUS_ERR);

    __atomic_store_n(&dma_active, 0, __ATOMIC_SEQ_CST);

    if (!completed) {
        kprintf("[ATA-DMA] Transfer timeout!\n");
        return -EIO;
    }

    return 0;
}

int ata_dma_read28(uint32_t lba, uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (!dma_available) return -ENOSYS;

    spin_lock(&dma_lock);

    int ret = ata_dma_transfer(lba, 0);
    if (ret == 0) {
        /* Copy from DMA bounce buffer to caller's buffer */
        memcpy(buf512, dma_buf, 512);
    }

    spin_unlock(&dma_lock);
    return ret;
}

int ata_dma_write28(uint32_t lba, const uint8_t* buf512) {
    if (!buf512) return -EFAULT;
    if (!dma_available) return -ENOSYS;

    spin_lock(&dma_lock);

    /* Copy caller's data into DMA bounce buffer */
    memcpy(dma_buf, buf512, 512);

    int ret = ata_dma_transfer(lba, 1);

    if (ret == 0) {
        /* Flush cache after write */
        outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
        (void)ata_wait_not_busy();
    }

    spin_unlock(&dma_lock);
    return ret;
}

int ata_dma_read_direct(uint32_t lba, uint32_t phys_buf, uint16_t byte_count) {
    if (!dma_available) return -ENOSYS;
    if (phys_buf == 0 || (phys_buf & 1)) return -EINVAL;
    if (byte_count == 0) byte_count = 512;

    spin_lock(&dma_lock);

    /* Save original PRDT and reprogram for zero-copy */
    uint32_t saved_addr = prdt[0].phys_addr;
    uint16_t saved_count = prdt[0].byte_count;
    prdt[0].phys_addr = phys_buf;
    prdt[0].byte_count = byte_count;

    int ret = ata_dma_transfer(lba, 0);

    /* Restore original PRDT for bounce-buffer mode */
    prdt[0].phys_addr = saved_addr;
    prdt[0].byte_count = saved_count;

    spin_unlock(&dma_lock);
    return ret;
}

int ata_dma_write_direct(uint32_t lba, uint32_t phys_buf, uint16_t byte_count) {
    if (!dma_available) return -ENOSYS;
    if (phys_buf == 0 || (phys_buf & 1)) return -EINVAL;
    if (byte_count == 0) byte_count = 512;

    spin_lock(&dma_lock);

    uint32_t saved_addr = prdt[0].phys_addr;
    uint16_t saved_count = prdt[0].byte_count;
    prdt[0].phys_addr = phys_buf;
    prdt[0].byte_count = byte_count;

    int ret = ata_dma_transfer(lba, 1);

    if (ret == 0) {
        outb((uint16_t)(ATA_IO_BASE + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
        (void)ata_wait_not_busy();
    }

    prdt[0].phys_addr = saved_addr;
    prdt[0].byte_count = saved_count;

    spin_unlock(&dma_lock);
    return ret;
}
