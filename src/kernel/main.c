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
#include "multiboot2.h"
#include "initrd.h"
#include "fs.h"
 #include "elf.h"
 #include "uaccess.h"

#include "kernel/boot_info.h"

#include "gdt.h"

#include "syscall.h"

#include "hal/cpu.h"

#if defined(__i386__)
#include "arch/x86/usermode.h"
#endif

#if defined(__i386__)
extern void x86_usermode_test_start(void);

static uint8_t ring0_trap_stack[16384] __attribute__((aligned(16)));

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
#endif

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
        const uintptr_t initrd_virt_base = 0xE0000000U;
        uintptr_t phys_start = (uintptr_t)bi->initrd_start & ~(uintptr_t)0xFFF;
        uintptr_t phys_end = (uintptr_t)bi->initrd_end;
        if (phys_end < (uintptr_t)bi->initrd_start) {
            phys_end = (uintptr_t)bi->initrd_start;
        }
        phys_end = (phys_end + 0xFFF) & ~(uintptr_t)0xFFF;

        uintptr_t size = phys_end - phys_start;
        uintptr_t pages = size >> 12;
        uintptr_t virt = initrd_virt_base;
        uintptr_t phys = phys_start;
        for (uintptr_t i = 0; i < pages; i++) {
            vmm_map_page((uint64_t)phys, (uint64_t)virt, VMM_FLAG_PRESENT | VMM_FLAG_RW);
            phys += 0x1000;
            virt += 0x1000;
        }

        uintptr_t initrd_virt = initrd_virt_base + ((uintptr_t)bi->initrd_start - phys_start);
        fs_root = initrd_init((uint32_t)initrd_virt);
    }

#if defined(__i386__)
    if (fs_root) {
        uintptr_t entry = 0;
        uintptr_t user_sp = 0;
        if (elf32_load_user_from_initrd("init.elf", &entry, &user_sp) == 0) {
            uart_print("[ELF] starting init.elf\n");

            uart_print("[ELF] user_range_ok(entry)=");
            uart_put_char(user_range_ok((const void*)entry, 1) ? '1' : '0');
            uart_print(" user_range_ok(stack)=");
            uart_put_char(user_range_ok((const void*)(user_sp - 16), 16) ? '1' : '0');
            uart_print("\n");

            hal_cpu_set_kernel_stack((uintptr_t)&ring0_trap_stack[sizeof(ring0_trap_stack)]);
            x86_enter_usermode(entry, user_sp);
        }
    }

    if (bi && cmdline_has_token(bi->cmdline, "ring3")) {
        x86_usermode_test_start();
    }
#endif
    
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
