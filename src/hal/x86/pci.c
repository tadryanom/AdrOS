#include "pci.h"
#include "console.h"
#include "utils.h"
#include "io.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static struct pci_device pci_devices[PCI_MAX_DEVICES];
static int pci_device_count = 0;

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1U << 31)
                     | ((uint32_t)bus << 16)
                     | ((uint32_t)(slot & 0x1F) << 11)
                     | ((uint32_t)(func & 0x07) << 8)
                     | ((uint32_t)offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (1U << 31)
                     | ((uint32_t)bus << 16)
                     | ((uint32_t)(slot & 0x1F) << 11)
                     | ((uint32_t)(func & 0x07) << 8)
                     | ((uint32_t)offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_scan_func(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t reg0 = pci_config_read(bus, slot, func, 0x00);
    uint16_t vendor = (uint16_t)(reg0 & 0xFFFF);
    uint16_t device = (uint16_t)(reg0 >> 16);

    if (vendor == 0xFFFF) return;
    if (pci_device_count >= PCI_MAX_DEVICES) return;

    struct pci_device* d = &pci_devices[pci_device_count];
    d->bus = bus;
    d->slot = slot;
    d->func = func;
    d->vendor_id = vendor;
    d->device_id = device;

    uint32_t reg2 = pci_config_read(bus, slot, func, 0x08);
    d->class_code = (uint8_t)(reg2 >> 24);
    d->subclass   = (uint8_t)(reg2 >> 16);
    d->prog_if    = (uint8_t)(reg2 >> 8);

    uint32_t reg3 = pci_config_read(bus, slot, func, 0x0C);
    d->header_type = (uint8_t)(reg3 >> 16);

    for (int i = 0; i < 6; i++) {
        d->bar[i] = pci_config_read(bus, slot, func, (uint8_t)(0x10 + i * 4));
    }

    uint32_t reg_irq = pci_config_read(bus, slot, func, 0x3C);
    d->irq_line = (uint8_t)(reg_irq & 0xFF);

    pci_device_count++;
}

static void pci_scan_slot(uint8_t bus, uint8_t slot) {
    uint32_t reg0 = pci_config_read(bus, slot, 0, 0x00);
    if ((reg0 & 0xFFFF) == 0xFFFF) return;

    pci_scan_func(bus, slot, 0);

    uint32_t reg3 = pci_config_read(bus, slot, 0, 0x0C);
    uint8_t header_type = (uint8_t)(reg3 >> 16);
    if (header_type & 0x80) {
        for (uint8_t func = 1; func < 8; func++) {
            pci_scan_func(bus, slot, func);
        }
    }
}

static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        pci_scan_slot(bus, slot);
    }
}

void pci_init(void) {
    pci_device_count = 0;

    uint32_t reg3 = pci_config_read(0, 0, 0, 0x0C);
    uint8_t header_type = (uint8_t)(reg3 >> 16);

    if (header_type & 0x80) {
        for (uint8_t func = 0; func < 8; func++) {
            uint32_t r = pci_config_read(0, 0, func, 0x00);
            if ((r & 0xFFFF) == 0xFFFF) continue;
            pci_scan_bus(func);
        }
    } else {
        pci_scan_bus(0);
    }

    kprintf("[PCI] Enumerated %d device(s)\n", pci_device_count);

    for (int i = 0; i < pci_device_count; i++) {
        struct pci_device* d = &pci_devices[i];
        kprintf("  %x:%x class=%x:%x\n",
                (unsigned)d->vendor_id, (unsigned)d->device_id,
                (unsigned)d->class_code, (unsigned)d->subclass);
    }
}

int pci_get_device_count(void) {
    return pci_device_count;
}

const struct pci_device* pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) return 0;
    return &pci_devices[index];
}

const struct pci_device* pci_find_device(uint16_t vendor, uint16_t device) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device)
            return &pci_devices[i];
    }
    return 0;
}

const struct pci_device* pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass)
            return &pci_devices[i];
    }
    return 0;
}
