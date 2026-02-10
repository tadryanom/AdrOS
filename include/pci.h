#ifndef PCI_H
#define PCI_H

#include <stdint.h>

struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint32_t bar[6];
    uint8_t  irq_line;
};

#define PCI_MAX_DEVICES 32

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

void pci_init(void);
int  pci_get_device_count(void);
const struct pci_device* pci_get_device(int index);
const struct pci_device* pci_find_device(uint16_t vendor, uint16_t device);
const struct pci_device* pci_find_class(uint8_t class_code, uint8_t subclass);

#endif
