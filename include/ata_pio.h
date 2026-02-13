#ifndef ATA_PIO_H
#define ATA_PIO_H

#include <stdint.h>
#include <stddef.h>

/* ATA drive identifiers */
#define ATA_DEV_PRIMARY_MASTER    0
#define ATA_DEV_PRIMARY_SLAVE     1
#define ATA_DEV_SECONDARY_MASTER  2
#define ATA_DEV_SECONDARY_SLAVE   3
#define ATA_MAX_DRIVES            4

/* Initialize both ATA channels and probe all 4 drives.
 * Returns 0 if at least one drive was found, negative errno otherwise. */
int ata_pio_init(void);

/* Returns 1 if the given drive was detected during init. */
int ata_pio_drive_present(int drive);

/* Read one 512-byte sector from the specified drive at the given LBA.
 * Returns 0 on success, negative errno on failure. */
int ata_pio_read28(int drive, uint32_t lba, uint8_t* buf512);

/* Write one 512-byte sector to the specified drive at the given LBA.
 * Returns 0 on success, negative errno on failure. */
int ata_pio_write28(int drive, uint32_t lba, const uint8_t* buf512);

uint32_t ata_pio_sector_size(void);

/* Map device name ("hda".."hdd") to drive ID. Returns -1 if invalid. */
int ata_name_to_drive(const char* name);

/* Map drive ID to device name. Returns NULL if invalid. */
const char* ata_drive_to_name(int drive);

#endif
