#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "uart_console.h"
#include <stddef.h>

struct process* current_process = NULL;
struct process* ready_queue_head = NULL;
struct process* ready_queue_tail = NULL;
static uint32_t next_pid = 1;

void process_init(void) {
    uart_print("[SCHED] Initializing Multitasking...\n");

    // Initial Kernel Thread (PID 0)
    struct process* kernel_proc = (struct process*)pmm_alloc_page();
    
    kernel_proc->pid = 0;
    kernel_proc->state = PROCESS_RUNNING;
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_proc->cr3));
    
    current_process = kernel_proc;
    ready_queue_head = kernel_proc;
    ready_queue_tail = kernel_proc;
    kernel_proc->next = kernel_proc;
}

/* 
 * Wrapper to start a new thread safely.
 * Since new threads don't return from 'context_switch', 
 * they miss the 'sti' instruction there. We must enable it here.
 */
void thread_wrapper(void (*fn)(void)) {
    __asm__ volatile("sti"); // Enable interrupts for the new thread!
    fn();                    // Run the task
    
    // If task returns, kill it (loop forever for now)
    uart_print("[SCHED] Thread exited.\n");
    for(;;) __asm__("hlt");
}

struct process* process_create_kernel(void (*entry_point)(void)) {
    struct process* proc = (struct process*)pmm_alloc_page();
    if (!proc) return NULL;

    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->cr3 = current_process->cr3; 
    
    // Allocate Kernel Stack
    void* stack_phys = pmm_alloc_page();
    
    // Assumption: We have identity map OR P2V works for this range.
    // For robustness in Higher Half, convert phys to virt if needed.
    // Since we map 0xC0000000 -> 0x0, adding 0xC0000000 gives us the virt address.
    uint32_t stack_virt = (uint32_t)stack_phys + 0xC0000000;
    
    proc->kernel_stack = (uint32_t*)stack_virt;
    
    // Top of stack
    uint32_t* sp = (uint32_t*)((uint8_t*)stack_virt + 4096);
    
    /* 
     * Forge the stack for context_switch
     * We want it to "return" to thread_wrapper, with entry_point as arg.
     * Stack Layout: [EIP] [Arg for Wrapper]
     */
     
    // Push Argument for thread_wrapper
    *--sp = (uint32_t)entry_point;
    
    // Push Return Address (EIP) - Where context_switch jumps to
    *--sp = (uint32_t)thread_wrapper;
    
    // Push Registers expected by context_switch (EBP, EBX, ESI, EDI)
    *--sp = 0; // EBP
    *--sp = 0; // EBX
    *--sp = 0; // ESI
    *--sp = 0; // EDI
    
    proc->esp = (uint32_t)sp;

    // Add to queue
    proc->next = ready_queue_head;
    ready_queue_tail->next = proc;
    ready_queue_tail = proc;

    return proc;
}

void schedule(void) {
    // Critical Section: Disable Interrupts
    __asm__ volatile("cli");

    if (!current_process) {
        __asm__ volatile("sti");
        return;
    }

    struct process* prev = current_process;
    struct process* next = current_process->next;
    
    if (prev == next) {
        __asm__ volatile("sti");
        return; 
    }

    current_process = next;
    current_process->state = PROCESS_RUNNING;
    
    // Switch!
    context_switch(&prev->esp, current_process->esp);
    
    // We are back! (Task resumed)
    // Re-enable interrupts
    __asm__ volatile("sti");
}
