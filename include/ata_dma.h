#ifndef ATA_DMA_H
#define ATA_DMA_H

#include <stdint.h>

/* ATA channel indices */
#define ATA_CHANNEL_PRIMARY   0
#define ATA_CHANNEL_SECONDARY 1
#define ATA_NUM_CHANNELS      2

/* Try to initialize ATA Bus Master DMA for the given channel.
 * Returns 0 on success, negative errno on failure (no PCI IDE, etc.).
 * If DMA init fails, PIO mode remains available. */
int ata_dma_init(int channel);

/* Returns 1 if DMA is available and initialized for the given channel. */
int ata_dma_available(int channel);

/* DMA read: read one sector (512 bytes) at LBA into buf.
 * channel: 0=primary, 1=secondary.  slave: 0=master, 1=slave.
 * buf must be a kernel virtual address.
 * Returns 0 on success, negative errno on failure. */
int ata_dma_read28(int channel, int slave, uint32_t lba, uint8_t* buf512);

/* DMA write: write one sector (512 bytes) from buf to LBA.
 * Returns 0 on success, negative errno on failure. */
int ata_dma_write28(int channel, int slave, uint32_t lba, const uint8_t* buf512);

/* Zero-copy DMA: read/write using a caller-provided physical address.
 * phys_buf must be 32-bit aligned, below 4GB, and not cross a 64KB boundary.
 * Returns 0 on success, negative errno on failure. */
int ata_dma_read_direct(int channel, int slave, uint32_t lba,
                        uint32_t phys_buf, uint16_t byte_count);
int ata_dma_write_direct(int channel, int slave, uint32_t lba,
                         uint32_t phys_buf, uint16_t byte_count);

#endif
