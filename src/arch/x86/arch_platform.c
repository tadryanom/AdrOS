#include "arch/arch_platform.h"

#include "elf.h"
#include "fs.h"
#include "keyboard.h"
#include "syscall.h"
#include "timer.h"
#include "console.h"
#include "uaccess.h"
#include "vga_console.h"
#include "vmm.h"

#include "process.h"
#include "heap.h"

#include "hal/cpu.h"
#include "hal/usermode.h"
#include "kernel/cmdline.h"

#if defined(__i386__)
#include "arch/x86/acpi.h"
#include "arch/x86/lapic.h"
#include "arch/x86/ioapic.h"
#include "arch/x86/smp.h"
#include "arch/x86/percpu.h"
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
        kprintf("[ELF] fs_root missing\n");
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    uintptr_t entry = 0;
    uintptr_t user_sp = 0;
    uintptr_t user_as = 0;
    uintptr_t heap_brk = 0;
    const char* init_path = cmdline_init_path();
    if (elf32_load_user_from_initrd(init_path, &entry, &user_sp, &user_as, &heap_brk) != 0) {
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    current_process->addr_space = user_as;
    current_process->heap_start = heap_brk;
    current_process->heap_break = heap_brk;
    vmm_as_activate(user_as);

    /* Open /dev/console as fd 0, 1, 2 — mirrors Linux kernel_init:
     *   sys_open("/dev/console", O_RDWR, 0);
     *   sys_dup(0); sys_dup(0);                                     */
    {
        fs_node_t* con = vfs_lookup("/dev/console");
        if (con) {
            struct file* f = (struct file*)kmalloc(sizeof(*f));
            if (f) {
                f->node = con;
                f->offset = 0;
                f->flags = 2; /* O_RDWR */
                f->refcount = 3;
                current_process->files[0] = f;
                current_process->files[1] = f;
                current_process->files[2] = f;
                kprintf("[INIT] opened /dev/console as fd 0/1/2\n");
            }
        } else {
            kprintf("[INIT] WARNING: /dev/console not found\n");
        }
    }

    kprintf("[ELF] starting %s\n", init_path);

    kprintf("[ELF] user_range_ok(entry)=%c user_range_ok(stack)=%c\n",
            user_range_ok((const void*)entry, 1) ? '1' : '0',
            user_range_ok((const void*)(user_sp - 16), 16) ? '1' : '0');

    hal_cpu_set_kernel_stack((uintptr_t)&ring0_trap_stack[sizeof(ring0_trap_stack)]);

    if (hal_usermode_enter(entry, user_sp) < 0) {
        kprintf("[USER] usermode enter not supported on this architecture.\n");
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    for (;;) hal_cpu_idle();
}
#endif

int arch_platform_setup(const struct boot_info* bi) {
#if defined(__i386__)
    vmm_init();

    /* Enable VGA text console only if we are NOT in linear framebuffer mode.
     * When GRUB provides a linear framebuffer (fb_type==1), the VGA text
     * buffer at 0xB8000 is inactive — serial console carries all output. */
    if (!bi || bi->fb_type != 1) {
        vga_init();
        vga_set_color(0x0A, 0x00);
        console_enable_vga(1);
        kprintf("[AdrOS] Kernel Initialized (VGA text mode).\n");
    } else {
        kprintf("[AdrOS] Kernel Initialized (framebuffer %ux%ux%u, VGA text disabled).\n",
                (unsigned)bi->fb_width, (unsigned)bi->fb_height, (unsigned)bi->fb_bpp);
    }

    syscall_init();

    /* Parse ACPI tables (MADT) to discover CPU topology and IOAPIC addresses */
    acpi_init();

    /* Initialize LAPIC + IOAPIC (replaces legacy PIC 8259).
     * If APIC is not available, PIC remains active from idt_init(). */
    if (lapic_init()) {
        if (ioapic_init()) {
            uint32_t bsp_id = lapic_get_id();
            /* Route ISA IRQs through IOAPIC:
             * IRQ 0  (PIT/Timer)      -> IDT vector 32
             * IRQ 1  (Keyboard)       -> IDT vector 33
             * IRQ 4  (COM1 UART)      -> IDT vector 36
             * IRQ 11 (E1000 NIC)      -> IDT vector 43
             * IRQ 14 (ATA primary)    -> IDT vector 46
             * IRQ 15 (ATA secondary)  -> IDT vector 47 */
            ioapic_route_irq(0,  32, (uint8_t)bsp_id);
            ioapic_route_irq(1,  33, (uint8_t)bsp_id);
            ioapic_route_irq(4,  36, (uint8_t)bsp_id); /* COM1 serial */
            ioapic_route_irq_level(11, 43, (uint8_t)bsp_id); /* E1000 NIC (PCI: level-triggered, active-low) */
            ioapic_route_irq(14, 46, (uint8_t)bsp_id); /* ATA primary */
            ioapic_route_irq(15, 47, (uint8_t)bsp_id); /* ATA secondary */

            /* Now that IOAPIC routes are live, disable the legacy PIC.
             * This must happen AFTER IOAPIC is configured to avoid
             * a window where no interrupt controller handles IRQs. */
            pic_disable();
        }

        /* Phase 1: Enumerate CPUs from ACPI MADT */
        smp_enumerate();

        /* Initialize per-CPU data and GDT entries (must be before APs start) */
        percpu_init();
        percpu_setup_gs(0);

        /* Phase 2: Send INIT-SIPI-SIPI to wake APs */
        smp_start_aps();
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

static void ring3_test_thread(void) {
    x86_usermode_test_start();
    for (;;) hal_cpu_idle();
}

void arch_platform_usermode_test_start(void) {
#if defined(__i386__)
    process_create_kernel(ring3_test_thread);
#endif
}
