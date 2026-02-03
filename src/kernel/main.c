#include <stdint.h>
#include "vga_console.h"
#include "uart_console.h"
#include "pmm.h"
#include "vmm.h"
#include "idt.h"
#include "io.h"
#include "process.h"

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will run into trouble"
#endif

// Simple Keyboard Handler Stub
void keyboard_handler(struct registers* regs) {
    (void)regs;
    // Read scan code from port 0x60
    uint8_t scancode = inb(0x60);
    
    // If top bit is set, it's a key release. Ignore.
    if (!(scancode & 0x80)) {
        uart_print("[KEY] Pressed!\n");
    }
}

void task_a(void) {
    uart_print("Task A Started!\n");
    for(;;) {
        uart_print("A");
        for(volatile int i=0; i<5000000; i++);
        schedule();
    }
}

void task_b(void) {
    uart_print("Task B Started!\n");
    for(;;) {
        uart_print("B");
        for(volatile int i=0; i<5000000; i++);
        schedule();
    }
}

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
    
    // 4. Initialize Interrupts (x86)
    uart_print("[AdrOS] Initializing IDT...\n");
    idt_init();
    
    register_interrupt_handler(33, keyboard_handler);
    
    // 5. Initialize Multitasking
    uart_print("[AdrOS] Initializing Scheduler...\n");
    process_init();
    
    process_create_kernel(task_a);
    process_create_kernel(task_b);
    
#else
    uart_print("[WARN] VMM/IDT/Sched not implemented for this architecture yet.\n");
#endif

    uart_print("Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!\n");
    uart_print("System Halted. Starting Multitasking...\n");

    // Infinite loop acting as Idle Task
    for(;;) {
        // uart_print(".");
        schedule();
        
        #if defined(__i386__) || defined(__x86_64__)
        __asm__("hlt");
        #endif
    }
}
