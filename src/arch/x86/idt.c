#include "arch/x86/idt.h"
#include "arch/x86/lapic.h"
#include "io.h"
#include "console.h"
#include "process.h"
#include "spinlock.h"
#include "uaccess.h"
#include "vmm.h"
#include "syscall.h"
#include "signal.h"
#include <stddef.h>

#include "arch/x86/signal.h"

#define IDT_ENTRIES 256
#define IRQ_CHAIN_POOL_SIZE 32

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr idtp;

struct irq_chain_node {
    isr_handler_t handler;
    struct irq_chain_node* next;
};

static struct irq_chain_node irq_chain_pool[IRQ_CHAIN_POOL_SIZE];
static struct irq_chain_node* irq_chain_heads[IDT_ENTRIES];

/* Legacy single-handler array kept for backward compatibility.
 * New registrations via register_interrupt_handler go through the chain. */
isr_handler_t interrupt_handlers[IDT_ENTRIES];

static spinlock_t idt_handlers_lock = {0};

static struct irq_chain_node* irq_chain_alloc(void) {
    for (int i = 0; i < IRQ_CHAIN_POOL_SIZE; i++) {
        if (irq_chain_pool[i].handler == 0) {
            return &irq_chain_pool[i];
        }
    }
    return 0;
}

// Extern prototypes for Assembly stubs
extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0(); extern void irq1(); extern void irq2(); extern void irq3();
extern void irq4(); extern void irq5(); extern void irq6(); extern void irq7();
extern void irq8(); extern void irq9(); extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

extern void isr128();
extern void isr255();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags; // Present(0x80) | DPL(00) | Type(0xE = 32bit Int Gate) -> 0x8E
}

static void deliver_signals_to_usermode(struct registers* regs) {
    if (!regs) return;
    if (!current_process) return;
    if ((regs->cs & 3U) != 3U) return;

    const uint32_t pending = current_process->sig_pending_mask;
    const uint32_t blocked = current_process->sig_blocked_mask;
    uint32_t ready = pending & ~blocked;
    if (!ready) return;

    int sig = -1;
    for (int i = 1; i < PROCESS_MAX_SIG; i++) {
        if ((ready & (1U << (uint32_t)i)) != 0U) {
            sig = i;
            break;
        }
    }
    if (sig < 0) return;

    current_process->sig_pending_mask &= ~(1U << (uint32_t)sig);

    const struct sigaction act = current_process->sigactions[sig];
    const uintptr_t h = (act.sa_flags & SA_SIGINFO) ? act.sa_sigaction : act.sa_handler;
    if (h == 1) {
        return;
    }

    if (h == 0) {
        if (sig == 11) {
            process_exit_notify(128 + sig);
            __asm__ volatile("sti");
            schedule();
            for (;;) __asm__ volatile("hlt");
        }
        return;
    }

    // Build a sigframe + a tiny user trampoline that calls SYSCALL_SIGRETURN.
    // Stack layout at handler entry (regs->useresp):
    //   non-SA_SIGINFO:
    //     [esp+0] return address -> trampoline
    //     [esp+4] int sig
    //   SA_SIGINFO:
    //     [esp+0] return address -> trampoline
    //     [esp+4] int sig
    //     [esp+8] siginfo_t*
    //     [esp+12] ucontext_t*
    // Below that: trampoline code bytes, below that: siginfo + ucontext + sigframe.

    struct sigframe f;
    f.magic = SIGFRAME_MAGIC;
    f.saved = *regs;

    const uint32_t tramp_size = 14U;
    const uint32_t callframe_size = (act.sa_flags & SA_SIGINFO) ? 16U : 8U;

    struct siginfo info;
    info.si_signo = sig;
    info.si_code = 1;
    info.si_addr = (sig == 11) ? (void*)current_process->last_fault_addr : NULL;

    struct ucontext uctx;
    uctx.reserved = 0;

    const uint32_t base = regs->useresp - (callframe_size + tramp_size + (uint32_t)sizeof(info) + (uint32_t)sizeof(uctx) + (uint32_t)sizeof(f));
    const uint32_t retaddr_slot = base;
    const uint32_t tramp_addr = base + callframe_size;
    const uint32_t siginfo_addr = tramp_addr + tramp_size;
    const uint32_t uctx_addr = siginfo_addr + (uint32_t)sizeof(info);
    const uint32_t sigframe_addr = uctx_addr + (uint32_t)sizeof(uctx);

    // Trampoline bytes:
    //   mov eax, SYSCALL_SIGRETURN
    //   mov ebx, <sigframe_addr>
    //   int 0x80
    //   jmp .
    uint8_t tramp[14];
    tramp[0] = 0xB8;
    {
        const uint32_t no = (uint32_t)SYSCALL_SIGRETURN;
        tramp[1] = (uint8_t)(no & 0xFFU);
        tramp[2] = (uint8_t)((no >> 8) & 0xFFU);
        tramp[3] = (uint8_t)((no >> 16) & 0xFFU);
        tramp[4] = (uint8_t)((no >> 24) & 0xFFU);
    }
    tramp[5] = 0xBB;
    // tramp[6..9] patched with sigframe address below
    tramp[10] = 0xCD;
    tramp[11] = 0x80;
    tramp[12] = 0xEB;
    tramp[13] = 0xFE;

    tramp[6] = (uint8_t)(sigframe_addr & 0xFFU);
    tramp[7] = (uint8_t)((sigframe_addr >> 8) & 0xFFU);
    tramp[8] = (uint8_t)((sigframe_addr >> 16) & 0xFFU);
    tramp[9] = (uint8_t)((sigframe_addr >> 24) & 0xFFU);

    if (copy_to_user((void*)(uintptr_t)sigframe_addr, &f, sizeof(f)) < 0 ||
        copy_to_user((void*)(uintptr_t)siginfo_addr, &info, sizeof(info)) < 0 ||
        copy_to_user((void*)(uintptr_t)uctx_addr, &uctx, sizeof(uctx)) < 0 ||
        copy_to_user((void*)(uintptr_t)tramp_addr, tramp, sizeof(tramp)) < 0) {
        const int SIG_SEGV = 11;
        process_exit_notify(128 + SIG_SEGV);
        __asm__ volatile("sti");
        schedule();
        for (;;) __asm__ volatile("hlt");
    }

    if ((act.sa_flags & SA_SIGINFO) == 0) {
        uint32_t callframe[2];
        callframe[0] = tramp_addr;
        callframe[1] = (uint32_t)sig;
        if (copy_to_user((void*)(uintptr_t)retaddr_slot, callframe, sizeof(callframe)) < 0) {
            const int SIG_SEGV = 11;
            process_exit_notify(128 + SIG_SEGV);
            __asm__ volatile("sti");
            schedule();
            for (;;) __asm__ volatile("hlt");
        }
    } else {
        uint32_t callframe[4];
        callframe[0] = tramp_addr;
        callframe[1] = (uint32_t)sig;
        callframe[2] = siginfo_addr;
        callframe[3] = uctx_addr;
        if (copy_to_user((void*)(uintptr_t)retaddr_slot, callframe, sizeof(callframe)) < 0) {
            const int SIG_SEGV = 11;
            process_exit_notify(128 + SIG_SEGV);
            __asm__ volatile("sti");
            schedule();
            for (;;) __asm__ volatile("hlt");
        }
    }

    regs->useresp = retaddr_slot;
    regs->eip = (uint32_t)h;
}

/* Reconfigure the PIC to remap IRQs from 0-15 to 32-47 */
void pic_remap(void) {
    uint8_t a1, a2;

    a1 = inb(0x21); // Save masks
    a2 = inb(0xA1);

    outb(0x20, 0x11); // Start Init (ICW1)
    outb(0xA0, 0x11);

    outb(0x21, 0x20); // Master offset: 32 (0x20) (ICW2)
    outb(0xA1, 0x28); // Slave offset: 40 (0x28)

    outb(0x21, 0x04); // Tell Master about Slave (ICW3)
    outb(0xA1, 0x02); // Tell Slave its cascade identity

    outb(0x21, 0x01); // 8086 mode (ICW4)
    outb(0xA1, 0x01);

    outb(0x21, a1);   // Restore masks
    outb(0xA1, a2);
}

void idt_init(void) {
    kprintf("[IDT] Initializing Interrupts...\n");
    
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (uint32_t)&idt;

    // Clear memory
    for (int i=0; i<IDT_ENTRIES; i++) {
        // Default to 0, non-present
        idt[i].base_lo = 0;
        idt[i].sel = 0;
        idt[i].always0 = 0;
        idt[i].flags = 0;
        interrupt_handlers[i] = NULL;
    }

    // Remap PIC
    pic_remap();

    // Install Exception Handlers (0-31)
    // 0x08 is our Kernel Code Segment
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0xEE);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E); // Page Fault!
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    // Install IRQ Handlers (32-47)
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E); // Timer
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E); // Keyboard
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // Syscall gate (int 0x80) must be callable from user mode (DPL=3)
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    // IPI reschedule vector (0xFD = 253) — wakes idle APs to run schedule()
    extern void isr253(void);
    idt_set_gate(253, (uint32_t)isr253, 0x08, 0x8E);

    // LAPIC spurious interrupt vector (must have an IDT entry or CPU triple-faults)
    idt_set_gate(255, (uint32_t)isr255, 0x08, 0x8E);

    // Load IDT
    __asm__ volatile("lidt %0" : : "m"(idtp));

    kprintf("[IDT] Loaded.\n");
}

void idt_load_ap(void) {
    __asm__ volatile("lidt %0" : : "m"(idtp));
}

void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    uintptr_t flags = spin_lock_irqsave(&idt_handlers_lock);

    /* If this vector has no handler yet, use the fast legacy slot */
    if (!interrupt_handlers[n] && !irq_chain_heads[n]) {
        interrupt_handlers[n] = handler;
        spin_unlock_irqrestore(&idt_handlers_lock, flags);
        return;
    }

    /* Migrate legacy handler to chain if needed */
    if (interrupt_handlers[n] && !irq_chain_heads[n]) {
        struct irq_chain_node* first = irq_chain_alloc();
        if (first) {
            first->handler = interrupt_handlers[n];
            first->next = 0;
            irq_chain_heads[n] = first;
        }
        interrupt_handlers[n] = 0;
    }

    /* Add new handler to chain */
    struct irq_chain_node* node = irq_chain_alloc();
    if (node) {
        node->handler = handler;
        node->next = irq_chain_heads[n];
        irq_chain_heads[n] = node;
    }

    spin_unlock_irqrestore(&idt_handlers_lock, flags);
}

void unregister_interrupt_handler(uint8_t n, isr_handler_t handler) {
    uintptr_t flags = spin_lock_irqsave(&idt_handlers_lock);

    /* Check legacy slot */
    if (interrupt_handlers[n] == handler) {
        interrupt_handlers[n] = 0;
        spin_unlock_irqrestore(&idt_handlers_lock, flags);
        return;
    }

    /* Search chain */
    struct irq_chain_node** pp = &irq_chain_heads[n];
    while (*pp) {
        if ((*pp)->handler == handler) {
            struct irq_chain_node* victim = *pp;
            *pp = victim->next;
            victim->handler = 0;
            victim->next = 0;
            break;
        }
        pp = &(*pp)->next;
    }

    /* If only one handler left in chain, migrate back to legacy slot */
    if (irq_chain_heads[n] && !irq_chain_heads[n]->next) {
        interrupt_handlers[n] = irq_chain_heads[n]->handler;
        irq_chain_heads[n]->handler = 0;
        irq_chain_heads[n] = 0;
    }

    spin_unlock_irqrestore(&idt_handlers_lock, flags);
}

#include "utils.h"

// ... imports ...

void print_reg(const char* name, uint32_t val) {
    kprintf("%s: %x  ", name, val);
}

// The Main Handler called by assembly
void isr_handler(struct registers* regs) {
    // LAPIC spurious interrupt (vector 255): do NOT send EOI per Intel spec
    if (regs->int_no == 255) {
        return;
    }

    // IPI reschedule (vector 253): send EOI and call schedule()
    if (regs->int_no == 253) {
        lapic_eoi();
        extern void schedule(void);
        schedule();
        return;
    }

    // Send EOI for IRQs (32-47) BEFORE calling the handler.
    // This is critical: the timer handler calls schedule() which may
    // context-switch away. If EOI is deferred until after the handler,
    // it never executes and the LAPIC blocks all further interrupts.
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (lapic_is_enabled()) {
            lapic_eoi();
        } else {
            if (regs->int_no >= 40) {
                outb(0xA0, 0x20); // Slave EOI
            }
            outb(0x20, 0x20); // Master EOI
        }
    }

    // Check if we have custom handler(s)
    if (irq_chain_heads[regs->int_no]) {
        /* Shared IRQ: call all chained handlers */
        struct irq_chain_node* node = irq_chain_heads[regs->int_no];
        while (node) {
            if (node->handler) node->handler(regs);
            node = node->next;
        }
    } else if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        // If Exception (0-31), Panic
        if (regs->int_no < 32) {
            // User-mode exceptions (ring 3): deliver SIGSEGV instead of panicking.
            // Handles #GP (13), #PF (14), and other faults from user code.
            if ((regs->cs & 3U) == 3U && regs->int_no != 14) {
                const int SIG_SEGV = 11;
                if (current_process) {
                    current_process->last_fault_addr = regs->eip;
                    current_process->sig_pending_mask |= (1U << (uint32_t)SIG_SEGV);
                }
                deliver_signals_to_usermode(regs);
                return;
            }

            if (regs->int_no == 14) {
                // If page fault came from ring3, convert it into a SIGSEGV delivery.
                // Default action for SIGSEGV will terminate the process, but a user
                // handler installed via sigaction() must be respected.
                uint32_t cr2;
                __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

                if ((regs->cs & 3U) == 3U) {
                    // Check for Copy-on-Write fault (write to read-only CoW page).
                    // Error code bit 1 = caused by a write.
                    if ((regs->err_code & 0x2) && vmm_handle_cow_fault((uintptr_t)cr2)) {
                        return;  // CoW resolved, resume user process.
                    }

                    const int SIG_SEGV = 11;
                    if (current_process) {
                        current_process->last_fault_addr = (uintptr_t)cr2;
                        current_process->sig_pending_mask |= (1U << (uint32_t)SIG_SEGV);
                    }
                    deliver_signals_to_usermode(regs);
                    return;
                }

                // Kernel-mode page faults during copy_{to,from}_user should not panic.
                if (uaccess_try_recover((uintptr_t)cr2, regs)) {
                    return;
                }
            }

            __asm__ volatile("cli"); // Stop everything
            
            kprintf("\n\n!!! KERNEL PANIC !!!\n");
            kprintf("Exception Number: %u\n", (unsigned)regs->int_no);
            
            kprintf("registers:\n");
            print_reg("EAX", regs->eax); print_reg("EBX", regs->ebx); print_reg("ECX", regs->ecx); print_reg("EDX", regs->edx);
            kprintf("\n");
            print_reg("ESI", regs->esi); print_reg("EDI", regs->edi); print_reg("EBP", regs->ebp); print_reg("ESP", regs->esp);
            kprintf("\n");
            print_reg("EIP", regs->eip); print_reg("CS ", regs->cs);  print_reg("EFLAGS", regs->eflags);
            kprintf("\n");

            // Print Page Fault specifics
            if (regs->int_no == 14) {
                uint32_t cr2;
                __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
                kprintf("\nPAGE FAULT at address: 0x%x\n", cr2);
            }
            
            kprintf("\nSystem Halted.\n");
            for(;;) __asm__("hlt");
        }
    }

    deliver_signals_to_usermode(regs);
}
