#include <stdint.h>
#include "vga_console.h"
#include "uart_console.h"
#include "pmm.h"
#include "vmm.h"
#include "idt.h"
#include "io.h"
#include "process.h"
#include "keyboard.h"
#include "shell.h"
#include "heap.h"
#include "timer.h"
#include "fs.h"
#include "initrd.h"
#include "multiboot2.h"

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will run into trouble"
#endif

/*
 * Kernel Entry Point
 * Arguments are passed from boot.S (architecture specific)
 */
void kernel_main(unsigned long magic, unsigned long addr) {
    
    // 1. Initialize Console (UART works everywhere)
    uart_init();
    uart_print("\n[AdrOS] Booting...\n");
    
    uint32_t initrd_location = 0;

#if defined(__i386__) || defined(__x86_64__)
    vga_init();
    vga_set_color(0x0A, 0x00); 
    vga_print("[AdrOS] Kernel Initialized (VGA).\n");
    
    // Check Multiboot2 Magic
    if (magic != 0x36d76289) {
        uart_print("[ERR] Invalid Multiboot2 Magic!\n");
    } else {
        uart_print("[OK] Multiboot2 Magic Confirmed.\n");
        
        // Search for InitRD Module
        struct multiboot_tag *tag;
        // Addr is physical. We need to access it. 
        // Bootloader puts it in low memory. We have identity map.
        // But let's be safe.
        // Assuming addr is accessible.
        
        for (tag = (struct multiboot_tag *)(addr + 8);
           tag->type != MULTIBOOT_TAG_TYPE_END;
           tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;
                initrd_location = mod->mod_start; // Physical
                uart_print("[BOOT] Found InitRD module!\n");
            }
        }
    }
#endif

    uart_print("[AdrOS] Initializing PMM...\n");
    
    // 2. Initialize Physical Memory Manager
    pmm_init((void*)addr);
    
    // 3. Initialize Virtual Memory Manager
    uart_print("[AdrOS] Initializing VMM...\n");
#if defined(__i386__)
    vmm_init(); 
    
    // 4. Initialize Kernel Heap
    kheap_init();
    
    // 5. Initialize FS (if InitRD found)
    if (initrd_location) {
        // Convert physical to virtual (Higher Half)
        // P2V macro is in vmm.h or locally defined? 
        // Let's assume PMM/VMM setup allows access via P2V
        // We need to define P2V here or include it
        #define P2V(x) ((uintptr_t)(x) + 0xC0000000)
        
        uint32_t virt_loc = P2V(initrd_location);
        fs_root = initrd_init(virt_loc);
    } else {
        uart_print("[WARN] No InitRD found. Filesystem will be empty.\n");
    }
    
    // 6. Initialize Interrupts (x86)
    uart_print("[AdrOS] Initializing IDT...\n");
    idt_init();
    
    // 7. Initialize Drivers
    keyboard_init();
    
    // 8. Initialize Multitasking
    uart_print("[AdrOS] Initializing Scheduler...\n");
    process_init();
    
    // 9. Start Timer (Preemption!) - 50Hz
    timer_init(50);
    
    // Start Shell as the main interaction loop
    shell_init();
    
#else
    uart_print("[WARN] VMM/IDT/Sched not implemented for this architecture yet.\n");
#endif

    uart_print("Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!\n");

    // Infinite loop acting as Idle Task
    for(;;) {
        // HLT puts CPU to sleep until next interrupt (Timer or Keyboard)
        #if defined(__i386__) || defined(__x86_64__)
        __asm__("hlt");
        #elif defined(__aarch64__)
        __asm__("wfi");
        #elif defined(__riscv)
        __asm__("wfi");
        #endif
    }
}
