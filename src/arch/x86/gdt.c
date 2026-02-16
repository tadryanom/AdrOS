#include "arch/x86/gdt.h"
#include "arch/x86/smp.h"

#include "console.h"
 #include "utils.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

extern void gdt_flush(uint32_t gdt_ptr_addr);
extern void tss_flush(uint16_t tss_selector);

/* 6 base + 16 percpu GS + 1 user TLS + 16 per-CPU TSS = 39 max */
#define GDT_MAX_ENTRIES 40
/* AP TSS entries start at GDT slot 23 (after user TLS at 22) */
#define TSS_AP_GDT_BASE 23

static struct gdt_entry gdt[GDT_MAX_ENTRIES];
struct gdt_ptr gp;
static struct tss_entry tss_array[SMP_MAX_CPUS];

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);

    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_set_gate_ext(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    if (num < 0 || num >= GDT_MAX_ENTRIES) return;
    gdt_set_gate(num, base, limit, access, gran);
    /* Reload GDT limit to include new entries */
    gp.limit = (uint16_t)(sizeof(struct gdt_entry) * GDT_MAX_ENTRIES - 1);
    __asm__ volatile("lgdt %0" : : "m"(gp));
}

static void tss_write(uint32_t gdt_idx, uint32_t cpu, uint16_t kernel_ss, uint32_t kernel_esp) {
    struct tss_entry* t = &tss_array[cpu];
    uintptr_t base = (uintptr_t)t;
    uint32_t limit = (uint32_t)(sizeof(*t) - 1);

    gdt_set_gate((int)gdt_idx, (uint32_t)base, limit, 0x89, 0x00);

    for (size_t i = 0; i < sizeof(*t); i++) {
        ((uint8_t*)t)[i] = 0;
    }

    t->ss0 = kernel_ss;
    t->esp0 = kernel_esp;
    t->iomap_base = (uint16_t)sizeof(*t);
}

extern void x86_sysenter_set_kernel_stack(uintptr_t esp0);

void tss_set_kernel_stack(uintptr_t esp0) {
    /* Determine which CPU we're on and update that CPU's TSS */
    extern uint32_t lapic_get_id(void);
    extern int lapic_is_enabled(void);
    uint32_t cpu = 0;
    if (lapic_is_enabled()) {
        extern uint32_t smp_current_cpu(void);
        cpu = smp_current_cpu();
    }
    if (cpu >= SMP_MAX_CPUS) cpu = 0;
    tss_array[cpu].esp0 = (uint32_t)esp0;
    x86_sysenter_set_kernel_stack(esp0);
}

void tss_init_ap(uint32_t cpu_index) {
    if (cpu_index == 0 || cpu_index >= SMP_MAX_CPUS) return;
    uint32_t gdt_idx = TSS_AP_GDT_BASE + (cpu_index - 1);
    tss_write(gdt_idx, cpu_index, 0x10, 0);
    uint16_t sel = (uint16_t)(gdt_idx * 8);
    tss_flush(sel);
}

void gdt_init(void) {
    kprintf("[GDT] Initializing GDT/TSS...\n");

    gp.limit = (uint16_t)(sizeof(struct gdt_entry) * GDT_MAX_ENTRIES - 1);
    gp.base = (uint32_t)(uintptr_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);

    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    tss_write(5, 0, 0x10, 0);

    gdt_flush((uint32_t)(uintptr_t)&gp);
    tss_flush(0x28);

    kprintf("[GDT] Loaded.\n");
}
