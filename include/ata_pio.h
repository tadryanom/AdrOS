#ifndef ATA_PIO_H
#define ATA_PIO_H

#include <stdint.h>
#include <stddef.h>

int ata_pio_init_primary_master(void);
int ata_pio_read28(uint32_t lba, uint8_t* buf512);
int ata_pio_write28(uint32_t lba, const uint8_t* buf512);
uint32_t ata_pio_sector_size(void);

#endif
