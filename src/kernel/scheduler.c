#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "uart_console.h"
#include "timer.h" // Need access to current tick usually, but we pass it in wake_check
#include "spinlock.h"
#include "hal/cpu.h"
#include <stddef.h>

struct process* current_process = NULL;
struct process* ready_queue_head = NULL;
struct process* ready_queue_tail = NULL;
static uint32_t next_pid = 1;

static spinlock_t sched_lock = {0};

static void* pmm_alloc_page_low(void) {
    // Bring-up safety: ensure we allocate from the identity-mapped area (0-4MB)
    // until we have a full kernel virtual mapping for arbitrary phys pages.
    for (int tries = 0; tries < 1024; tries++) {
        void* p = pmm_alloc_page();
        if (!p) return 0;
        if ((uint32_t)p < 0x00400000) {
            return p;
        }
        // Not safe to touch yet; put it back.
        pmm_free_page(p);
    }
    return 0;
}

void process_init(void) {
    uart_print("[SCHED] Initializing Multitasking...\n");

    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    // Initial Kernel Thread (PID 0) - IDLE TASK
    struct process* kernel_proc = (struct process*)pmm_alloc_page_low();
    
    kernel_proc->pid = 0;
    kernel_proc->state = PROCESS_RUNNING;
    kernel_proc->wake_at_tick = 0;
    kernel_proc->addr_space = hal_cpu_get_address_space();
    
    current_process = kernel_proc;
    ready_queue_head = kernel_proc;
    ready_queue_tail = kernel_proc;
    kernel_proc->next = kernel_proc;

    // Best effort: set esp0 to current stack until we have a dedicated kernel stack for PID 0
    uintptr_t cur_esp = hal_cpu_get_stack_pointer();
    hal_cpu_set_kernel_stack(cur_esp);

    spin_unlock_irqrestore(&sched_lock, flags);
}

void thread_wrapper(void (*fn)(void)) {
    hal_cpu_enable_interrupts();
    fn();
    uart_print("[SCHED] Thread exited.\n");
    for(;;) hal_cpu_idle();
}

struct process* process_create_kernel(void (*entry_point)(void)) {
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    struct process* proc = (struct process*)pmm_alloc_page_low();
    if (!proc) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }

    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->addr_space = current_process->addr_space;
    proc->wake_at_tick = 0;
    
    void* stack_phys = pmm_alloc_page_low();
    if (!stack_phys) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }

    // Until we guarantee a linear phys->virt mapping, use the identity-mapped address
    // for kernel thread stacks during bring-up.
    uint32_t stack_addr = (uint32_t)stack_phys;
    proc->kernel_stack = (uint32_t*)stack_addr;
    
    uint32_t* sp = (uint32_t*)((uint8_t*)stack_addr + 4096);
    
    *--sp = (uint32_t)entry_point;
    *--sp = (uint32_t)thread_wrapper;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    
    proc->sp = (uintptr_t)sp;

    proc->next = ready_queue_head;
    ready_queue_tail->next = proc;
    ready_queue_tail = proc;

    spin_unlock_irqrestore(&sched_lock, flags);
    return proc;
}

// Find next READY process
struct process* get_next_ready_process(void) {
    struct process* iterator = current_process->next;
    
    // Safety Break to prevent infinite loop if list broken
    int count = 0;
    while (iterator != current_process && count < 100) {
        if (iterator->state == PROCESS_READY) {
            return iterator;
        }
        iterator = iterator->next;
        count++;
    }
    
    // If current is ready/running, return it.
    if (current_process->state == PROCESS_RUNNING || current_process->state == PROCESS_READY)
        return current_process;
        
    // If EVERYONE is sleeping, we must return the IDLE task (PID 0)
    // Assuming PID 0 is always in the list.
    // Search specifically for PID 0
    iterator = current_process->next;
    while (iterator->pid != 0) {
        iterator = iterator->next;
        if (iterator == current_process) break; // Should not happen
    }
    return iterator; // Return idle task
}

void schedule(void) {
    uintptr_t irq_flags = irq_save();
    spin_lock(&sched_lock);

    if (!current_process) {
        spin_unlock(&sched_lock);
        irq_restore(irq_flags);
        return;
    }

    struct process* prev = current_process;
    struct process* next = get_next_ready_process();
    
    if (prev == next) {
        spin_unlock(&sched_lock);
        irq_restore(irq_flags);
        return; 
    }

    // Only change state to READY if it was RUNNING. 
    // If it was SLEEPING/BLOCKED, leave it as is.
    if (prev->state == PROCESS_RUNNING) {
        prev->state = PROCESS_READY;
    }

    current_process = next;
    current_process->state = PROCESS_RUNNING;

    // For ring3->ring0 transitions, esp0 must point to the top of the kernel stack.
    if (current_process->kernel_stack) {
        hal_cpu_set_kernel_stack((uintptr_t)current_process->kernel_stack + 4096);
    }

    spin_unlock(&sched_lock);
    
    context_switch(&prev->sp, current_process->sp);

    irq_restore(irq_flags);
}

void process_sleep(uint32_t ticks) {
    // We need current tick count. 
    // For simplicity, let's just use a extern or pass it.
    // But usually sleep() is called by process logic.
    // Let's assume we read the global tick from timer.h accessor (TODO)
    // Or we just add 'ticks' to current.
    
    // Quick fix: declare extern tick from timer.c
    extern uint32_t get_tick_count(void);
    
    uint32_t current_tick = get_tick_count();

    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    current_process->wake_at_tick = current_tick + ticks;
    current_process->state = PROCESS_SLEEPING;
    
    spin_unlock_irqrestore(&sched_lock, flags);

    // Force switch immediately. Since current state is SLEEPING, schedule() will pick someone else.
    schedule();

    // When we return here, we woke up!
}

void process_wake_check(uint32_t current_tick) {
    // Called by Timer ISR
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    struct process* iter = ready_queue_head;
    
    // Iterate all processes (Circular list)
    // Warning: O(N) inside ISR. Not ideal for 1000 processes.
    
    if (!iter) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return;
    }
    
    struct process* start = iter;
    do {
        if (iter->state == PROCESS_SLEEPING) {
            if (current_tick >= iter->wake_at_tick) {
                iter->state = PROCESS_READY;
                // uart_print("Woke up PID "); 
            }
        }
        iter = iter->next;
    } while (iter != start);

    spin_unlock_irqrestore(&sched_lock, flags);
}
