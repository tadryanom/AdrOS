#ifndef ARCH_X86_ACPI_H
#define ARCH_X86_ACPI_H

#include <stdint.h>

/* RSDP (Root System Description Pointer) — ACPI 1.0 */
struct acpi_rsdp {
    char     signature[8];   /* "RSD PTR " */
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;       /* 0 = ACPI 1.0, 2 = ACPI 2.0+ */
    uint32_t rsdt_address;
} __attribute__((packed));

/* SDT header — common to all ACPI tables */
struct acpi_sdt_header {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

/* RSDT (Root System Description Table) */
struct acpi_rsdt {
    struct acpi_sdt_header header;
    uint32_t entries[];      /* array of 32-bit physical pointers to other SDTs */
} __attribute__((packed));

/* MADT (Multiple APIC Description Table) */
struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t lapic_address;  /* Physical address of LAPIC */
    uint32_t flags;          /* bit 0: dual 8259 PICs installed */
} __attribute__((packed));

/* MADT entry header */
struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

/* MADT entry types */
#define MADT_TYPE_LAPIC          0
#define MADT_TYPE_IOAPIC         1
#define MADT_TYPE_ISO            2  /* Interrupt Source Override */
#define MADT_TYPE_NMI_SOURCE     3
#define MADT_TYPE_LAPIC_NMI      4

/* MADT: Processor Local APIC */
struct madt_lapic {
    struct madt_entry_header header;
    uint8_t  acpi_processor_id;
    uint8_t  apic_id;
    uint32_t flags;          /* bit 0: processor enabled */
} __attribute__((packed));

#define MADT_LAPIC_ENABLED  (1U << 0)

/* MADT: I/O APIC */
struct madt_ioapic {
    struct madt_entry_header header;
    uint8_t  ioapic_id;
    uint8_t  reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;       /* Global System Interrupt base */
} __attribute__((packed));

/* MADT: Interrupt Source Override */
struct madt_iso {
    struct madt_entry_header header;
    uint8_t  bus_source;     /* always 0 (ISA) */
    uint8_t  irq_source;     /* ISA IRQ number */
    uint32_t gsi;            /* Global System Interrupt */
    uint16_t flags;          /* polarity + trigger mode */
} __attribute__((packed));

/* Maximum CPUs we support */
#define ACPI_MAX_CPUS    16

/* Parsed ACPI info */
struct acpi_info {
    uint8_t  num_cpus;
    uint8_t  bsp_id;                        /* BSP LAPIC ID */
    uint8_t  cpu_lapic_ids[ACPI_MAX_CPUS];  /* LAPIC IDs of all CPUs */
    uint8_t  cpu_enabled[ACPI_MAX_CPUS];    /* 1 if CPU is enabled */

    uint32_t ioapic_address;                /* Physical address of IOAPIC */
    uint8_t  ioapic_id;
    uint32_t ioapic_gsi_base;

    uint32_t lapic_address;                 /* Physical address of LAPIC (from MADT) */
};

/* Find and parse ACPI tables. Returns 0 on success, -1 on failure. */
int acpi_init(void);

/* Get parsed ACPI info. Valid only after acpi_init() succeeds. */
const struct acpi_info* acpi_get_info(void);

#endif
