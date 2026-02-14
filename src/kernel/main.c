#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "vga_console.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "keyboard.h"
#include "kconsole.h"
#include "heap.h"
#include "timer.h"
#include "initrd.h"
#include "fs.h"

#include "kernel/boot_info.h"
#include "kernel/init.h"

#include "syscall.h"

#include "arch/arch_platform.h"

#include "hal/cpu.h"
#include "hal/cpu_features.h"
#include "shm.h"
#include "net.h"


/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#warning "You are not using a cross-compiler, you may run into trouble"
#endif

/*
 * Kernel Entry Point
 * Arguments are passed from boot.S (architecture specific)
 */
void kernel_main(const struct boot_info* bi) {
    console_init();

    hal_cpu_detect_features();
    hal_cpu_print_features();

    kprintf("[AdrOS] Initializing PMM...\n");
    
    // 2. Initialize Physical Memory Manager
    pmm_init((void*)(bi ? bi->arch_boot_info : 0));
    
    // 3. Initialize Virtual Memory Manager
    kprintf("[AdrOS] Initializing VMM...\n");
    if (arch_platform_setup(bi) < 0) {
        kprintf("[WARN] VMM/IDT/Sched not implemented for this architecture yet.\n");
        goto done;
    }

    // 4. Initialize Kernel Heap
    kheap_init();

    // 5. Initialize Shared Memory IPC
    shm_init();
    
    // 6b. Initialize KASLR PRNG
    {
        extern void kaslr_init(void);
        kaslr_init();
    }

    // 7. Initialize Multitasking
    kprintf("[AdrOS] Initializing Scheduler...\n");
    process_init();
    
    // 7b. Initialize vDSO shared page
    {
        extern void vdso_init(void);
        vdso_init();
    }

    // 8. Start Timer (Preemption!) - 100Hz (like Linux CONFIG_HZ=100)
    timer_init(TIMER_HZ);

    hal_cpu_enable_interrupts();

    int init_ret = init_start(bi);
    
    if (init_ret < 0) {
        kconsole_enter();
    }
    
done:
    kprintf("Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!\n");

    // Infinite loop acting as Idle Task
    // RX is interrupt-driven (e1000_rx_thread), no polling needed.
    for(;;) {
        hal_cpu_idle();
    }
}
