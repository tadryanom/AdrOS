#ifndef KERNEL_VA_MAP_H
#define KERNEL_VA_MAP_H

/*
 * Centralized kernel virtual address map for MMIO / DMA / special regions.
 *
 * All fixed-VA allocations MUST be listed here to prevent collisions.
 * The kernel image is loaded at KERNEL_VIRT_BASE (0xC0000000) and BSS
 * can extend past 0xC0200000 with large static pools (lwIP, FAT, etc.).
 *
 * Layout (sorted by VA):
 *
 *   0xC0000000 .. ~0xC0203000  Kernel .text/.data/.bss (variable)
 *   0xC0201000                 IOAPIC MMIO (1 page)
 *   0xC0280000                 vDSO shared page (1 page)
 *   0xC0300000 .. 0xC030FFFF   ACPI temp window (16 pages)
 *   0xC0320000                 ATA DMA PRDT (1 page)
 *   0xC0321000                 ATA DMA bounce buffer (1 page)
 *   0xC0330000 .. 0xC034FFFF   E1000 MMIO (32 pages, 128 KB)
 *   0xC0350000                 E1000 TX descriptor ring (1 page)
 *   0xC0351000                 E1000 RX descriptor ring (1 page)
 *   0xC0352000 .. 0xC0361FFF   E1000 TX buffers (16 pages)
 *   0xC0362000 .. 0xC0371FFF   E1000 RX buffers (16 pages)
 *   0xC0400000                 LAPIC MMIO (1 page)
 *   0xC8000000 ..              Kernel stacks (guard + 8KB per thread)
 *   0xD0000000 ..              Kernel heap (10 MB)
 *   0xDC000000 ..              Initrd / generic phys mapping (up to 64 MB)
 *   0xE0000000 ..              Framebuffer mapping (up to 16 MB)
 */

/* IOAPIC (arch/x86/ioapic.c) */
#define KVA_IOAPIC          0xC0201000U

/* vDSO shared page (kernel/vdso.c) */
#define KVA_VDSO            0xC0280000U

/* ACPI temp mapping window — 16 pages (arch/x86/acpi.c) */
#define KVA_ACPI_TMP_BASE   0xC0300000U
#define KVA_ACPI_TMP_PAGES  16

/* ATA DMA (hal/x86/ata_dma.c) — two pages per channel (PRDT + bounce buf) */
#define KVA_ATA_DMA_PRDT_PRI  0xC0320000U
#define KVA_ATA_DMA_BUF_PRI   0xC0321000U
#define KVA_ATA_DMA_PRDT_SEC  0xC0322000U
#define KVA_ATA_DMA_BUF_SEC   0xC0323000U

/* E1000 NIC (drivers/e1000.c) */
#define KVA_E1000_MMIO      0xC0330000U
#define KVA_E1000_MMIO_PAGES 32
#define KVA_E1000_TX_DESC   0xC0350000U
#define KVA_E1000_RX_DESC   0xC0351000U
#define KVA_E1000_TX_BUF    0xC0352000U
#define KVA_E1000_RX_BUF    0xC0362000U

/* LAPIC (arch/x86/lapic.c) */
#define KVA_LAPIC           0xC0400000U

/* Initrd / generic physical range mapping (hal/x86/mm.c) */
#define KVA_PHYS_MAP        0xDC000000U

/* Framebuffer (drivers/vbe.c) — up to 16 MB for large resolutions */
#define KVA_FRAMEBUFFER     0xE0000000U

#endif
