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
#include "multiboot2.h"
#include "initrd.h"
#include "fs.h"

#include "kernel/boot_info.h"

#include "gdt.h"

#include "syscall.h"

#include "hal/cpu.h"

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#warning "You are not using a cross-compiler, you may run into trouble"
#endif

/*
 * Kernel Entry Point
 * Arguments are passed from boot.S (architecture specific)
 */
void kernel_main(const struct boot_info* bi) {
    
    uart_print("[AdrOS] Initializing PMM...\n");
    
    // 2. Initialize Physical Memory Manager
    pmm_init((void*)(bi ? bi->arch_boot_info : 0));
    
    // 3. Initialize Virtual Memory Manager
    uart_print("[AdrOS] Initializing VMM...\n");
#if defined(__i386__)
    vmm_init(); 

    // VGA console depends on higher-half mapping (VMM)
    vga_init();
    vga_set_color(0x0A, 0x00);
    vga_print("[AdrOS] Kernel Initialized (VGA).\n");
    
    // 4. Initialize Kernel Heap
    kheap_init();

    syscall_init();
    
    // 6. Initialize Drivers
    keyboard_init();
    
    // 7. Initialize Multitasking
    uart_print("[AdrOS] Initializing Scheduler...\n");
    process_init();
    
    // 8. Start Timer (Preemption!) - 50Hz
    timer_init(50);

    hal_cpu_enable_interrupts();

    // 9. Load InitRD (if available)
    if (bi && bi->initrd_start) {
        fs_root = initrd_init((uint32_t)bi->initrd_start);
    }
    
    // Start Shell as the main interaction loop
    shell_init();
    
#else
    uart_print("[WARN] VMM/IDT/Sched not implemented for this architecture yet.\n");
#endif

    uart_print("Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!\n");

    // Infinite loop acting as Idle Task
    for(;;) {
        hal_cpu_idle();
    }
}
