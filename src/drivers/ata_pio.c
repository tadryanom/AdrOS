#include "ata_pio.h"
#include "errno.h"

__attribute__((weak))
uint32_t ata_pio_sector_size(void) { return 512; }

__attribute__((weak))
int ata_pio_init_primary_master(void) { return -ENOSYS; }

__attribute__((weak))
int ata_pio_read28(uint32_t lba, uint8_t* buf512) { (void)lba; (void)buf512; return -ENOSYS; }

__attribute__((weak))
int ata_pio_write28(uint32_t lba, const uint8_t* buf512) { (void)lba; (void)buf512; return -ENOSYS; }
