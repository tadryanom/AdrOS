#include "ata_pio.h"
#include "errno.h"
#include "utils.h"

__attribute__((weak))
uint32_t ata_pio_sector_size(void) { return 512; }

__attribute__((weak))
int ata_pio_init(void) { return -ENOSYS; }

__attribute__((weak))
int ata_pio_drive_present(int drive) { (void)drive; return 0; }

__attribute__((weak))
int ata_pio_read28(int drive, uint32_t lba, uint8_t* buf512) {
    (void)drive; (void)lba; (void)buf512; return -ENOSYS;
}

__attribute__((weak))
int ata_pio_write28(int drive, uint32_t lba, const uint8_t* buf512) {
    (void)drive; (void)lba; (void)buf512; return -ENOSYS;
}

__attribute__((weak))
int ata_name_to_drive(const char* name) { (void)name; return -1; }

__attribute__((weak))
const char* ata_drive_to_name(int drive) { (void)drive; return 0; }

__attribute__((weak))
void ata_register_devfs(void) { }
