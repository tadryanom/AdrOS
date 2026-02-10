// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "pci.h"
#include "uart_console.h"

__attribute__((weak))
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    (void)bus; (void)slot; (void)func; (void)offset;
    return 0xFFFFFFFFU;
}

__attribute__((weak))
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    (void)bus; (void)slot; (void)func; (void)offset; (void)value;
}

__attribute__((weak))
void pci_init(void) {
    uart_print("[PCI] Not supported on this architecture.\n");
}

__attribute__((weak))
int pci_get_device_count(void) { return 0; }

__attribute__((weak))
const struct pci_device* pci_get_device(int index) { (void)index; return 0; }

__attribute__((weak))
const struct pci_device* pci_find_device(uint16_t vendor, uint16_t device) { (void)vendor; (void)device; return 0; }

__attribute__((weak))
const struct pci_device* pci_find_class(uint8_t class_code, uint8_t subclass) { (void)class_code; (void)subclass; return 0; }
