#include "arch/arch_platform.h"

#include "elf.h"
#include "fs.h"
#include "keyboard.h"
#include "syscall.h"
#include "timer.h"
#include "uart_console.h"
#include "console.h"
#include "uaccess.h"
#include "vga_console.h"
#include "vmm.h"

#include "process.h"

#include "hal/cpu.h"
#include "hal/usermode.h"

#if defined(__i386__)
#include "arch/x86/acpi.h"
#include "arch/x86/lapic.h"
#include "arch/x86/ioapic.h"
#include "arch/x86/smp.h"
#endif

#if defined(__i386__)
extern void x86_usermode_test_start(void);
#endif

#if defined(__i386__)
static uint8_t ring0_trap_stack[16384] __attribute__((aligned(16)));
#endif

#if defined(__i386__)
static void userspace_init_thread(void) {
    if (!fs_root) {
        uart_print("[ELF] fs_root missing\n");
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    uintptr_t entry = 0;
    uintptr_t user_sp = 0;
    uintptr_t user_as = 0;
    uintptr_t heap_brk = 0;
    if (elf32_load_user_from_initrd("/bin/init.elf", &entry, &user_sp, &user_as, &heap_brk) != 0) {
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    current_process->addr_space = user_as;
    current_process->heap_start = heap_brk;
    current_process->heap_break = heap_brk;
    vmm_as_activate(user_as);

    uart_print("[ELF] starting /bin/init.elf\n");

    uart_print("[ELF] user_range_ok(entry)=");
    uart_put_char(user_range_ok((const void*)entry, 1) ? '1' : '0');
    uart_print(" user_range_ok(stack)=");
    uart_put_char(user_range_ok((const void*)(user_sp - 16), 16) ? '1' : '0');
    uart_print("\n");

    hal_cpu_set_kernel_stack((uintptr_t)&ring0_trap_stack[sizeof(ring0_trap_stack)]);

    if (hal_usermode_enter(entry, user_sp) < 0) {
        uart_print("[USER] usermode enter not supported on this architecture.\n");
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    for (;;) hal_cpu_idle();
}
#endif

int arch_platform_setup(const struct boot_info* bi) {
    (void)bi;
#if defined(__i386__)
    vmm_init();

    vga_init();
    vga_set_color(0x0A, 0x00);
    console_enable_vga(1);
    kprintf("[AdrOS] Kernel Initialized (VGA).\n");

    syscall_init();

    /* Parse ACPI tables (MADT) to discover CPU topology and IOAPIC addresses */
    acpi_init();

    /* Initialize LAPIC + IOAPIC (replaces legacy PIC 8259).
     * If APIC is not available, PIC remains active from idt_init(). */
    if (lapic_init()) {
        if (ioapic_init()) {
            uint32_t bsp_id = lapic_get_id();
            /* Route ISA IRQs through IOAPIC:
             * IRQ 0 (PIT/Timer)    -> IDT vector 32
             * IRQ 1 (Keyboard)     -> IDT vector 33
             * IRQ 14 (ATA primary) -> IDT vector 46 */
            ioapic_route_irq(0,  32, (uint8_t)bsp_id);
            ioapic_route_irq(1,  33, (uint8_t)bsp_id);
            ioapic_route_irq(14, 46, (uint8_t)bsp_id);

            /* Now that IOAPIC routes are live, disable the legacy PIC.
             * This must happen AFTER IOAPIC is configured to avoid
             * a window where no interrupt controller handles IRQs. */
            pic_disable();
        }

        /* Bootstrap Application Processors (APs) via INIT-SIPI-SIPI */
        smp_init();
    }

    keyboard_init();

    return 0;
#else
    return -1;
#endif
}

int arch_platform_start_userspace(const struct boot_info* bi) {
    (void)bi;
#if defined(__i386__)
    const struct process* p = process_create_kernel(userspace_init_thread);
    if (!p) return -1;
    return 0;
#else
    return -1;
#endif
}

void arch_platform_usermode_test_start(void) {
#if defined(__i386__)
    x86_usermode_test_start();
#endif
}
