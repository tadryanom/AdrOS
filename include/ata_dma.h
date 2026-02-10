#ifndef ATA_DMA_H
#define ATA_DMA_H

#include <stdint.h>

/* Try to initialize ATA Bus Master DMA on the primary channel.
 * Returns 0 on success, negative errno on failure (no PCI IDE, etc.).
 * If DMA init fails, PIO mode remains available. */
int ata_dma_init(void);

/* Returns 1 if DMA is available and initialized. */
int ata_dma_available(void);

/* DMA read: read one sector (512 bytes) at LBA into buf.
 * buf must be a kernel virtual address (will be translated to physical).
 * Returns 0 on success, negative errno on failure. */
int ata_dma_read28(uint32_t lba, uint8_t* buf512);

/* DMA write: write one sector (512 bytes) from buf to LBA.
 * Returns 0 on success, negative errno on failure. */
int ata_dma_write28(uint32_t lba, const uint8_t* buf512);

#endif
