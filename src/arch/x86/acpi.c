#include "arch/x86/acpi.h"
#include "console.h"
#include "utils.h"
#include "vmm.h"

#include <stdint.h>
#include <stddef.h>

static struct acpi_info g_acpi_info;
static int g_acpi_valid = 0;

/* The first 16MB is identity-mapped during early boot (boot.S maps 0-16MB).
 * For addresses < 16MB we can use phys + 0xC0000000.
 * For addresses >= 16MB we must temporarily map them via VMM. */
#include "kernel_va_map.h"

#define KERNEL_VIRT_BASE 0xC0000000U
#define IDENTITY_LIMIT   0x01000000U  /* 16MB */

/* Temporary VA window for ACPI tables — see include/kernel_va_map.h */
#define ACPI_TMP_VA_BASE KVA_ACPI_TMP_BASE
#define ACPI_TMP_VA_PAGES KVA_ACPI_TMP_PAGES
static uint32_t acpi_tmp_mapped = 0;  /* bitmask of which pages are mapped */

/* Map a physical address and return a usable virtual pointer.
 * For addresses in the identity-mapped range, just add KERNEL_VIRT_BASE.
 * For others, temporarily map via VMM. */
static const void* acpi_map_phys(uintptr_t phys, size_t len) {
    if (phys + len <= IDENTITY_LIMIT) {
        return (const void*)(phys + KERNEL_VIRT_BASE);
    }

    /* Map all pages covering [phys, phys+len) into the temp VA window */
    uintptr_t page_start = phys & ~0xFFFU;
    uintptr_t page_end = (phys + len + 0xFFF) & ~0xFFFU;
    uint32_t num_pages = (uint32_t)((page_end - page_start) >> 12);

    if (num_pages > ACPI_TMP_VA_PAGES) {
        kprintf("[ACPI] Table too large to map.\n");
        return NULL;
    }

    for (uint32_t i = 0; i < num_pages; i++) {
        uintptr_t va = ACPI_TMP_VA_BASE + i * 0x1000;
        uintptr_t pa = page_start + i * 0x1000;
        vmm_map_page((uint64_t)pa, (uint64_t)va,
                     VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_NOCACHE);
        acpi_tmp_mapped |= (1U << i);
    }

    uintptr_t offset = phys - page_start;
    return (const void*)(ACPI_TMP_VA_BASE + offset);
}

/* Unmap all temporarily mapped ACPI pages */
static void acpi_unmap_all(void) {
    for (uint32_t i = 0; i < ACPI_TMP_VA_PAGES; i++) {
        if (acpi_tmp_mapped & (1U << i)) {
            vmm_unmap_page((uint64_t)(ACPI_TMP_VA_BASE + i * 0x1000));
        }
    }
    acpi_tmp_mapped = 0;
}

static int acpi_checksum(const void* ptr, size_t len) {
    const uint8_t* p = (const uint8_t*)ptr;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += p[i];
    return sum == 0;
}

/* Search for "RSD PTR " signature in a memory range (physical addresses) */
static const struct acpi_rsdp* find_rsdp_in_range(uintptr_t phys_start, uintptr_t phys_end) {
    /* RSDP is always 16-byte aligned */
    for (uintptr_t addr = phys_start; addr < phys_end; addr += 16) {
        const char* p = (const char*)(addr + KERNEL_VIRT_BASE);
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ') {
            const struct acpi_rsdp* rsdp = (const struct acpi_rsdp*)p;
            if (acpi_checksum(rsdp, 20)) {
                return rsdp;
            }
        }
    }
    return NULL;
}

static const struct acpi_rsdp* find_rsdp(void) {
    /* 1. Search EBDA (Extended BIOS Data Area) — first KB pointed to by BDA[0x40E] */
    uint16_t ebda_seg = *(const uint16_t*)(0x040E + KERNEL_VIRT_BASE);
    uintptr_t ebda_phys = (uintptr_t)ebda_seg << 4;
    if (ebda_phys >= 0x80000 && ebda_phys < 0xA0000) {
        const struct acpi_rsdp* r = find_rsdp_in_range(ebda_phys, ebda_phys + 1024);
        if (r) return r;
    }

    /* 2. Search BIOS ROM area: 0xE0000 - 0xFFFFF */
    return find_rsdp_in_range(0xE0000, 0x100000);
}

static int parse_madt(const struct acpi_madt* madt) {
    g_acpi_info.lapic_address = madt->lapic_address;

    const uint8_t* ptr = (const uint8_t*)madt + sizeof(struct acpi_madt);
    const uint8_t* end = (const uint8_t*)madt + madt->header.length;

    while (ptr + 2 <= end) {
        const struct madt_entry_header* eh = (const struct madt_entry_header*)ptr;
        if (eh->length < 2) break;
        if (ptr + eh->length > end) break;

        switch (eh->type) {
        case MADT_TYPE_LAPIC: {
            const struct madt_lapic* lapic = (const struct madt_lapic*)ptr;
            if (g_acpi_info.num_cpus < ACPI_MAX_CPUS) {
                uint8_t idx = g_acpi_info.num_cpus;
                g_acpi_info.cpu_lapic_ids[idx] = lapic->apic_id;
                g_acpi_info.cpu_enabled[idx] = (lapic->flags & MADT_LAPIC_ENABLED) ? 1 : 0;
                g_acpi_info.num_cpus++;
            }
            break;
        }
        case MADT_TYPE_IOAPIC: {
            const struct madt_ioapic* ioapic = (const struct madt_ioapic*)ptr;
            /* Use the first IOAPIC found */
            if (g_acpi_info.ioapic_address == 0) {
                g_acpi_info.ioapic_address = ioapic->ioapic_address;
                g_acpi_info.ioapic_id = ioapic->ioapic_id;
                g_acpi_info.ioapic_gsi_base = ioapic->gsi_base;
            }
            break;
        }
        case MADT_TYPE_ISO: {
            /* TODO: store interrupt source overrides for IRQ remapping */
            break;
        }
        default:
            break;
        }

        ptr += eh->length;
    }

    return 0;
}

int acpi_init(void) {
    memset(&g_acpi_info, 0, sizeof(g_acpi_info));

    const struct acpi_rsdp* rsdp = find_rsdp();
    if (!rsdp) {
        kprintf("[ACPI] RSDP not found.\n");
        return -1;
    }

    kprintf("[ACPI] RSDP found, revision=%d\n", (int)rsdp->revision);

    /* Get RSDT (ACPI 1.0 — 32-bit pointers).
     * The RSDT may be above the 16MB identity-mapped range, so use acpi_map_phys. */
    uintptr_t rsdt_phys = rsdp->rsdt_address;

    /* First map just the header to read the length */
    const struct acpi_sdt_header* rsdt_hdr =
        (const struct acpi_sdt_header*)acpi_map_phys(rsdt_phys, sizeof(struct acpi_sdt_header));
    if (!rsdt_hdr) {
        kprintf("[ACPI] Cannot map RSDT header.\n");
        return -1;
    }
    uint32_t rsdt_len = rsdt_hdr->length;
    acpi_unmap_all();

    /* Now map the full RSDT */
    const struct acpi_rsdt* rsdt =
        (const struct acpi_rsdt*)acpi_map_phys(rsdt_phys, rsdt_len);
    if (!rsdt) {
        kprintf("[ACPI] Cannot map full RSDT.\n");
        return -1;
    }

    if (!acpi_checksum(rsdt, rsdt_len)) {
        kprintf("[ACPI] RSDT checksum failed.\n");
        acpi_unmap_all();
        return -1;
    }

    /* Search for MADT ("APIC") in RSDT entries */
    uint32_t num_entries = (rsdt_len - sizeof(struct acpi_sdt_header)) / 4;
    uintptr_t madt_phys = 0;

    for (uint32_t i = 0; i < num_entries; i++) {
        uintptr_t entry_phys = rsdt->entries[i];
        acpi_unmap_all();

        const struct acpi_sdt_header* hdr =
            (const struct acpi_sdt_header*)acpi_map_phys(entry_phys, sizeof(struct acpi_sdt_header));
        if (!hdr) continue;
        if (hdr->signature[0] == 'A' && hdr->signature[1] == 'P' &&
            hdr->signature[2] == 'I' && hdr->signature[3] == 'C') {
            madt_phys = entry_phys;
            break;
        }

        /* Re-map RSDT for next iteration */
        acpi_unmap_all();
        rsdt = (const struct acpi_rsdt*)acpi_map_phys(rsdt_phys, rsdt_len);
        if (!rsdt) break;
    }
    acpi_unmap_all();

    if (!madt_phys) {
        kprintf("[ACPI] MADT not found.\n");
        return -1;
    }

    /* Map MADT header to get length, then map full table */
    const struct acpi_sdt_header* madt_hdr =
        (const struct acpi_sdt_header*)acpi_map_phys(madt_phys, sizeof(struct acpi_sdt_header));
    if (!madt_hdr) {
        kprintf("[ACPI] Cannot map MADT header.\n");
        return -1;
    }
    uint32_t madt_len = madt_hdr->length;
    acpi_unmap_all();

    const struct acpi_madt* madt =
        (const struct acpi_madt*)acpi_map_phys(madt_phys, madt_len);
    if (!madt) {
        kprintf("[ACPI] Cannot map full MADT.\n");
        return -1;
    }

    if (!acpi_checksum(madt, madt_len)) {
        kprintf("[ACPI] MADT checksum failed.\n");
        acpi_unmap_all();
        return -1;
    }

    if (parse_madt(madt) < 0) {
        kprintf("[ACPI] MADT parse failed.\n");
        acpi_unmap_all();
        return -1;
    }
    acpi_unmap_all();

    g_acpi_valid = 1;

    /* Print summary */
    kprintf("[ACPI] MADT: %u CPU(s), LAPIC=0x%x, IOAPIC=0x%x\n",
            (unsigned)g_acpi_info.num_cpus,
            (unsigned)g_acpi_info.lapic_address,
            (unsigned)g_acpi_info.ioapic_address);

    for (uint8_t i = 0; i < g_acpi_info.num_cpus; i++) {
        kprintf("[ACPI]   CPU %u: LAPIC ID=%u%s\n",
                (unsigned)i, (unsigned)g_acpi_info.cpu_lapic_ids[i],
                g_acpi_info.cpu_enabled[i] ? " (enabled)" : " (disabled)");
    }

    return 0;
}

const struct acpi_info* acpi_get_info(void) {
    if (!g_acpi_valid) return NULL;
    return &g_acpi_info;
}
