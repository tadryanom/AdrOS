 #include "arch/arch_early_setup.h"

#include "kernel/boot_info.h"

#include "gdt.h"
#include "idt.h"
#include "uart_console.h"

#include "multiboot2.h"

extern void kernel_main(const struct boot_info* bi);

static uint8_t multiboot_copy[65536];
static uint32_t multiboot_copy_size;

 void arch_early_setup(const struct arch_boot_args* args) {
    uart_init();
    uart_print("\n[AdrOS] Booting...\n");

    uint32_t magic = (uint32_t)(args ? args->a0 : 0);
    uintptr_t mbi_phys = (uintptr_t)(args ? args->a1 : 0);

    if (magic != 0x36d76289) {
        uart_print("[ERR] Invalid Multiboot2 Magic!\n");
    } else {
        uart_print("[OK] Multiboot2 Magic Confirmed.\n");
    }

    uart_print("[AdrOS] Initializing GDT/TSS...\n");
    gdt_init();

    uart_print("[AdrOS] Initializing IDT...\n");
    idt_init();

    struct boot_info bi;
    bi.arch_magic = magic;
    bi.arch_boot_info = 0;
    bi.initrd_start = 0;
    bi.initrd_end = 0;
    bi.cmdline = NULL;

    if (mbi_phys) {
        uint32_t total_size = *(volatile uint32_t*)mbi_phys;
        if (total_size >= 8) {
            multiboot_copy_size = total_size;
            if (multiboot_copy_size > sizeof(multiboot_copy)) {
                uart_print("[WARN] Multiboot2 info too large, truncating copy.\n");
                multiboot_copy_size = sizeof(multiboot_copy);
            }

            for (uint32_t i = 0; i < multiboot_copy_size; i++) {
                multiboot_copy[i] = *(volatile uint8_t*)(mbi_phys + i);
            }
            bi.arch_boot_info = (uintptr_t)multiboot_copy;
        }
    }

    if (bi.arch_boot_info) {
        struct multiboot_tag* tag;
        for (tag = (struct multiboot_tag*)((uint8_t*)bi.arch_boot_info + 8);
             tag->type != MULTIBOOT_TAG_TYPE_END;
             tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                struct multiboot_tag_module* mod = (struct multiboot_tag_module*)tag;
                bi.initrd_start = mod->mod_start;
                bi.initrd_end = mod->mod_end;
                break;
            }
            if (tag->type == MULTIBOOT_TAG_TYPE_CMDLINE) {
                struct multiboot_tag_string* s = (struct multiboot_tag_string*)tag;
                bi.cmdline = s->string;
            }
        }
    }

    kernel_main(&bi);

    for(;;) {
        __asm__ volatile("hlt");
    }
}
