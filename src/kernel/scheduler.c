#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "uart_console.h"
#include "timer.h" // Need access to current tick usually, but we pass it in wake_check
#include <stddef.h>

struct process* current_process = NULL;
struct process* ready_queue_head = NULL;
struct process* ready_queue_tail = NULL;
static uint32_t next_pid = 1;

void process_init(void) {
    uart_print("[SCHED] Initializing Multitasking...\n");

    // Initial Kernel Thread (PID 0) - IDLE TASK
    struct process* kernel_proc = (struct process*)pmm_alloc_page();
    
    kernel_proc->pid = 0;
    kernel_proc->state = PROCESS_RUNNING;
    kernel_proc->wake_at_tick = 0;
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_proc->cr3));
    
    current_process = kernel_proc;
    ready_queue_head = kernel_proc;
    ready_queue_tail = kernel_proc;
    kernel_proc->next = kernel_proc;
}

void thread_wrapper(void (*fn)(void)) {
    __asm__ volatile("sti"); 
    fn();
    uart_print("[SCHED] Thread exited.\n");
    for(;;) __asm__("hlt");
}

struct process* process_create_kernel(void (*entry_point)(void)) {
    struct process* proc = (struct process*)pmm_alloc_page();
    if (!proc) return NULL;

    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->cr3 = current_process->cr3; 
    proc->wake_at_tick = 0;
    
    void* stack_phys = pmm_alloc_page();
    uint32_t stack_virt = (uint32_t)stack_phys + 0xC0000000;
    proc->kernel_stack = (uint32_t*)stack_virt;
    
    uint32_t* sp = (uint32_t*)((uint8_t*)stack_virt + 4096);
    
    *--sp = (uint32_t)entry_point;
    *--sp = (uint32_t)thread_wrapper;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    
    proc->esp = (uint32_t)sp;

    proc->next = ready_queue_head;
    ready_queue_tail->next = proc;
    ready_queue_tail = proc;

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
    __asm__ volatile("cli");

    if (!current_process) {
        __asm__ volatile("sti");
        return;
    }

    struct process* prev = current_process;
    struct process* next = get_next_ready_process();
    
    if (prev == next) {
        __asm__ volatile("sti");
        return; 
    }

    // Only change state to READY if it was RUNNING. 
    // If it was SLEEPING/BLOCKED, leave it as is.
    if (prev->state == PROCESS_RUNNING) {
        prev->state = PROCESS_READY;
    }

    current_process = next;
    current_process->state = PROCESS_RUNNING;
    
    context_switch(&prev->esp, current_process->esp);
    
    __asm__ volatile("sti");
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
    
    __asm__ volatile("cli");
    current_process->wake_at_tick = current_tick + ticks;
    current_process->state = PROCESS_SLEEPING;
    
    // Force switch immediately
    // Since current state is SLEEPING, schedule() will pick someone else.
    // We call schedule() directly (but we need to re-enable interrupts inside schedule logic or before context switch return)
    // Our schedule() handles interrupt flag management, but we called CLI above.
    // schedule() calls CLI again (no-op) and then STI at end.
    
    // BUT we need to manually invoke the scheduler logic here because schedule() usually triggered by ISR.
    // Just calling schedule() works.
    
    schedule();
    
    // When we return here, we woke up!
    __asm__ volatile("sti");
}

void process_wake_check(uint32_t current_tick) {
    // Called by Timer ISR
    struct process* iter = ready_queue_head;
    
    // Iterate all processes (Circular list)
    // Warning: O(N) inside ISR. Not ideal for 1000 processes.
    
    if (!iter) return;
    
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
}
