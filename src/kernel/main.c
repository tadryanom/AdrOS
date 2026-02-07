#include <stdint.h>
#include <stddef.h>
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
#include "initrd.h"
#include "fs.h"

#include "kernel/boot_info.h"

#include "syscall.h"

#include "arch/arch_platform.h"

#include "hal/cpu.h"
#include "hal/mm.h"

static int cmdline_has_token(const char* cmdline, const char* token) {
    if (!cmdline || !token) return 0;

    for (size_t i = 0; cmdline[i] != 0; i++) {
        size_t j = 0;
        while (token[j] != 0 && cmdline[i + j] == token[j]) {
            j++;
        }
        if (token[j] == 0) {
            char before = (i == 0) ? ' ' : cmdline[i - 1];
            char after = cmdline[i + j];
            int before_ok = (before == ' ' || before == '\t');
            int after_ok = (after == 0 || after == ' ' || after == '\t');
            if (before_ok && after_ok) return 1;
        }
    }

    return 0;
}

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
    if (arch_platform_setup(bi) < 0) {
        uart_print("[WARN] VMM/IDT/Sched not implemented for this architecture yet.\n");
        goto done;
    }

    // 4. Initialize Kernel Heap
    kheap_init();
    
    // 7. Initialize Multitasking
    uart_print("[AdrOS] Initializing Scheduler...\n");
    process_init();
    
    // 8. Start Timer (Preemption!) - 50Hz
    timer_init(50);

    hal_cpu_enable_interrupts();

    // 9. Load InitRD (if available)
    if (bi && bi->initrd_start) {
        uintptr_t initrd_virt = 0;
        if (hal_mm_map_physical_range((uintptr_t)bi->initrd_start, (uintptr_t)bi->initrd_end,
                                      HAL_MM_MAP_RW, &initrd_virt) == 0) {
            fs_root = initrd_init((uint32_t)initrd_virt);
        } else {
            uart_print("[INITRD] Failed to map initrd physical range.\n");
        }
    }

    (void)arch_platform_start_userspace(bi);

    if (bi && cmdline_has_token(bi->cmdline, "ring3")) {
        arch_platform_usermode_test_start();
    }
    
    // Start Shell as the main interaction loop
    shell_init();
    
done:
    uart_print("Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!\n");

    // Infinite loop acting as Idle Task
    for(;;) {
        hal_cpu_idle();
    }
}
