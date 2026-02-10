#include "arch/x86/gdt.h"

#include "uart_console.h"
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

/* 6 base entries + up to SMP_MAX_CPUS per-CPU GS segments */
#define GDT_MAX_ENTRIES 24
static struct gdt_entry gdt[GDT_MAX_ENTRIES];
struct gdt_ptr gp;
static struct tss_entry tss;

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

static void tss_write(uint32_t idx, uint16_t kernel_ss, uint32_t kernel_esp) {
    uintptr_t base = (uintptr_t)&tss;
    uint32_t limit = (uint32_t)(sizeof(tss) - 1);

    gdt_set_gate((int)idx, (uint32_t)base, limit, 0x89, 0x00);

    for (size_t i = 0; i < sizeof(tss); i++) {
        ((uint8_t*)&tss)[i] = 0;
    }

    tss.ss0 = kernel_ss;
    tss.esp0 = kernel_esp;
    tss.iomap_base = (uint16_t)sizeof(tss);
}

extern void x86_sysenter_set_kernel_stack(uintptr_t esp0);

void tss_set_kernel_stack(uintptr_t esp0) {
    tss.esp0 = (uint32_t)esp0;
    x86_sysenter_set_kernel_stack(esp0);
}

void gdt_init(void) {
    uart_print("[GDT] Initializing GDT/TSS...\n");

    gp.limit = (uint16_t)(sizeof(struct gdt_entry) * GDT_MAX_ENTRIES - 1);
    gp.base = (uint32_t)(uintptr_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);

    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    tss_write(5, 0x10, 0);

    gdt_flush((uint32_t)(uintptr_t)&gp);
    tss_flush(0x28);

    uart_print("[GDT] Loaded.\n");
}
