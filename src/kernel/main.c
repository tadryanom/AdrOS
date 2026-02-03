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

#if defined(__i386__) || defined(__x86_64__)
    vga_init();
    vga_set_color(0x0A, 0x00); 
    vga_print("[AdrOS] Kernel Initialized (VGA).\n");
    
    // Check Multiboot2 Magic
    if (magic != 0x36d76289) {
        uart_print("[ERR] Invalid Multiboot2 Magic!\n");
    } else {
        uart_print("[OK] Multiboot2 Magic Confirmed.\n");
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
    
    // 5. Initialize Interrupts (x86)
    uart_print("[AdrOS] Initializing IDT...\n");
    idt_init();
    
    // 6. Initialize Drivers
    keyboard_init();
    
    // 7. Initialize Multitasking
    uart_print("[AdrOS] Initializing Scheduler...\n");
    process_init();
    
    // 8. Start Timer (Preemption!) - 50Hz
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
