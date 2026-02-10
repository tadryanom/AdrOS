#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "uart_console.h"
#include "timer.h" // Need access to current tick usually, but we pass it in wake_check
#include "spinlock.h"
#include "utils.h"
#include "errno.h"
#include "hal/cpu.h"
#if defined(__i386__)
#include "arch/x86/usermode.h"
#endif
#include <stddef.h>

struct process* current_process = NULL;
struct process* ready_queue_head = NULL;
struct process* ready_queue_tail = NULL;
static uint32_t next_pid = 1;

static spinlock_t sched_lock = {0};
static uintptr_t kernel_as = 0;

void thread_wrapper(void (*fn)(void));

static struct process* process_find_locked(uint32_t pid) {
    if (!ready_queue_head) return NULL;

    struct process* it = ready_queue_head;
    const struct process* const start = it;
    do {
        if (it->pid == pid) return it;
        it = it->next;
    } while (it && it != start);

    return NULL;
}

static void process_reap_locked(struct process* p) {
    if (!p) return;
    if (p->pid == 0) return;

    if (p == ready_queue_head && p == ready_queue_tail) {
        return;
    }

    if (p->next) {
        p->next->prev = p->prev;
    }
    if (p->prev) {
        p->prev->next = p->next;
    }

    if (p == ready_queue_head) {
        ready_queue_head = p->next;
    }
    if (p == ready_queue_tail) {
        ready_queue_tail = p->prev;
    }

    if (p->kernel_stack) {
        kfree(p->kernel_stack);
        p->kernel_stack = NULL;
    }

    if (p->addr_space && p->addr_space != kernel_as) {
        vmm_as_destroy(p->addr_space);
        p->addr_space = 0;
    }

    kfree(p);
}

static void process_close_all_files_locked(struct process* p) {
    if (!p) return;
    for (int fd = 0; fd < PROCESS_MAX_FILES; fd++) {
        struct file* f = p->files[fd];
        if (!f) continue;
        p->files[fd] = NULL;

        if (f->refcount > 0) {
            f->refcount--;
        }
        if (f->refcount == 0) {
            if (f->node) {
                vfs_close(f->node);
            }
            kfree(f);
        }
    }
}

int process_kill(uint32_t pid, int sig) {
    const int SIG_KILL = 9;
    if (pid == 0) return -EINVAL;

    if (sig <= 0 || sig >= PROCESS_MAX_SIG) return -EINVAL;

    if (current_process && current_process->pid == pid && sig == SIG_KILL) {
        process_exit_notify(128 + sig);
        hal_cpu_enable_interrupts();
        schedule();
        for (;;) hal_cpu_idle();
    }

    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    struct process* p = process_find_locked(pid);
    if (!p || p->pid == 0) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return -ESRCH;
    }

    if (p->state == PROCESS_ZOMBIE) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return 0;
    }

    if (sig == SIG_KILL) {
        process_close_all_files_locked(p);
        p->exit_status = 128 + sig;
        p->state = PROCESS_ZOMBIE;

        if (p->pid != 0) {
            struct process* parent = process_find_locked(p->parent_pid);
            if (parent && parent->state == PROCESS_BLOCKED && parent->waiting) {
                if (parent->wait_pid == -1 || parent->wait_pid == (int)p->pid) {
                    parent->wait_result_pid = (int)p->pid;
                    parent->wait_result_status = p->exit_status;
                    parent->state = PROCESS_READY;
                }
            }
        }
    } else {
        p->sig_pending_mask |= (1U << (uint32_t)sig);
        if (p->state == PROCESS_BLOCKED || p->state == PROCESS_SLEEPING) {
            p->state = PROCESS_READY;
        }
    }

    spin_unlock_irqrestore(&sched_lock, flags);
    return 0;
}

int process_waitpid(int pid, int* status_out, uint32_t options) {
    if (!current_process) return -ECHILD;

    const uint32_t WNOHANG = 1U;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&sched_lock);

        struct process* it = ready_queue_head;
        struct process* start = it;
        int found_child = 0;

        if (it) {
            do {
                if (it->parent_pid == current_process->pid) {
                    found_child = 1;
                    if (pid == -1 || (int)it->pid == pid) {
                        if (it->state == PROCESS_ZOMBIE) {
                            int retpid = (int)it->pid;
                            int st = it->exit_status;
                            process_reap_locked(it);
                            spin_unlock_irqrestore(&sched_lock, flags);
                            if (status_out) *status_out = st;
                            return retpid;
                        }
                    }
                }
                it = it->next;
            } while (it != start);
        }

        if (!found_child) {
            spin_unlock_irqrestore(&sched_lock, flags);
            return -ECHILD;
        }

        if ((options & WNOHANG) != 0) {
            spin_unlock_irqrestore(&sched_lock, flags);
            return 0;
        }

        current_process->waiting = 1;
        current_process->wait_pid = pid;
        current_process->wait_result_pid = -1;
        current_process->state = PROCESS_BLOCKED;

        spin_unlock_irqrestore(&sched_lock, flags);

        hal_cpu_enable_interrupts();
        schedule();

        if (current_process->wait_result_pid != -1) {
            int rp = current_process->wait_result_pid;
            int st = current_process->wait_result_status;

            uintptr_t flags2 = spin_lock_irqsave(&sched_lock);
            struct process* child = process_find_locked((uint32_t)rp);
            if (child && child->parent_pid == current_process->pid && child->state == PROCESS_ZOMBIE) {
                process_reap_locked(child);
            }
            spin_unlock_irqrestore(&sched_lock, flags2);

            current_process->waiting = 0;
            current_process->wait_pid = -1;
            current_process->wait_result_pid = -1;
            if (status_out) *status_out = st;
            return rp;
        }
    }
}

void process_exit_notify(int status) {
    if (!current_process) return;

    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    current_process->exit_status = status;
    current_process->state = PROCESS_ZOMBIE;

    if (current_process->pid != 0) {
        struct process* parent = process_find_locked(current_process->parent_pid);
        if (parent && parent->state == PROCESS_BLOCKED && parent->waiting) {
            if (parent->wait_pid == -1 || parent->wait_pid == (int)current_process->pid) {
                parent->wait_result_pid = (int)current_process->pid;
                parent->wait_result_status = status;
                parent->state = PROCESS_READY;
            }
        }
    }

    spin_unlock_irqrestore(&sched_lock, flags);
}

static void fork_child_trampoline(void) {
#if defined(__i386__)
    if (!current_process || !current_process->has_user_regs) {
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    if (current_process->addr_space) {
        vmm_as_activate(current_process->addr_space);
    }

    x86_enter_usermode_regs(&current_process->user_regs);
#else
    process_exit_notify(1);
    schedule();
    for (;;) hal_cpu_idle();
#endif
}

struct process* process_fork_create(uintptr_t child_as, const struct registers* child_regs) {
    if (!child_as || !child_regs) return NULL;

    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    struct process* proc = (struct process*)kmalloc(sizeof(*proc));
    if (!proc) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }
    memset(proc, 0, sizeof(*proc));

    proc->pid = next_pid++;
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->session_id = current_process ? current_process->session_id : proc->pid;
    proc->pgrp_id = current_process ? current_process->pgrp_id : proc->pid;
    proc->state = PROCESS_READY;
    proc->addr_space = child_as;
    proc->wake_at_tick = 0;
    proc->exit_status = 0;

    proc->waiting = 0;
    proc->wait_pid = -1;
    proc->wait_result_pid = -1;
    proc->wait_result_status = 0;

    if (current_process) {
        strcpy(proc->cwd, current_process->cwd);
    } else {
        strcpy(proc->cwd, "/");
    }

    proc->has_user_regs = 1;
    proc->user_regs = *child_regs;

    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        proc->files[i] = NULL;
    }

    void* stack = kmalloc(4096);
    if (!stack) {
        kfree(proc);
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }
    proc->kernel_stack = (uint32_t*)stack;

    uint32_t* sp = (uint32_t*)((uint8_t*)stack + 4096);
    *--sp = (uint32_t)fork_child_trampoline;
    *--sp = 0;
    *--sp = (uint32_t)thread_wrapper;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    proc->sp = (uintptr_t)sp;

    proc->next = ready_queue_head;
    proc->prev = ready_queue_tail;
    ready_queue_tail->next = proc;
    ready_queue_head->prev = proc;
    ready_queue_tail = proc;

    spin_unlock_irqrestore(&sched_lock, flags);
    return proc;
}

void process_init(void) {
    uart_print("[SCHED] Initializing Multitasking...\n");

    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    // Initial Kernel Thread (PID 0) - IDLE TASK
    struct process* kernel_proc = (struct process*)kmalloc(sizeof(*kernel_proc));
    if (!kernel_proc) {
        spin_unlock_irqrestore(&sched_lock, flags);
        uart_print("[SCHED] OOM allocating kernel process struct.\n");
        for(;;) hal_cpu_idle();
        __builtin_unreachable();
    }

    memset(kernel_proc, 0, sizeof(*kernel_proc));
    
    kernel_proc->pid = 0;
    kernel_proc->parent_pid = 0;
    kernel_proc->session_id = 0;
    kernel_proc->pgrp_id = 0;
    kernel_proc->state = PROCESS_RUNNING;
    kernel_proc->wake_at_tick = 0;
    kernel_proc->addr_space = hal_cpu_get_address_space();
    kernel_as = kernel_proc->addr_space;
    kernel_proc->exit_status = 0;
    kernel_proc->waiting = 0;
    kernel_proc->wait_pid = -1;
    kernel_proc->wait_result_pid = -1;
    kernel_proc->wait_result_status = 0;

    strcpy(kernel_proc->cwd, "/");

    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        kernel_proc->files[i] = NULL;
    }
    
    current_process = kernel_proc;
    ready_queue_head = kernel_proc;
    ready_queue_tail = kernel_proc;
    kernel_proc->next = kernel_proc;
    kernel_proc->prev = kernel_proc;

    // Best effort: set esp0 to current stack until we have a dedicated kernel stack for PID 0
    uintptr_t cur_esp = hal_cpu_get_stack_pointer();
    hal_cpu_set_kernel_stack(cur_esp);

    spin_unlock_irqrestore(&sched_lock, flags);
}

void thread_wrapper(void (*fn)(void)) {
    hal_cpu_enable_interrupts();
    fn();
    for(;;) hal_cpu_idle();
}

struct process* process_create_kernel(void (*entry_point)(void)) {
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    struct process* proc = (struct process*)kmalloc(sizeof(*proc));
    if (!proc) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }

    memset(proc, 0, sizeof(*proc));

    proc->pid = next_pid++;
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->session_id = current_process ? current_process->session_id : proc->pid;
    proc->pgrp_id = current_process ? current_process->pgrp_id : proc->pid;
    proc->state = PROCESS_READY;
    proc->addr_space = kernel_as ? kernel_as : (current_process ? current_process->addr_space : 0);
    proc->wake_at_tick = 0;
    proc->exit_status = 0;
    proc->waiting = 0;
    proc->wait_pid = -1;
    proc->wait_result_pid = -1;
    proc->wait_result_status = 0;

    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        proc->files[i] = NULL;
    }
    
    void* stack = kmalloc(4096);
    if (!stack) {
        kfree(proc);
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }

    proc->kernel_stack = (uint32_t*)stack;
    
    uint32_t* sp = (uint32_t*)((uint8_t*)stack + 4096);
    
    *--sp = (uint32_t)entry_point;
    *--sp = 0;
    *--sp = (uint32_t)thread_wrapper;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    
    proc->sp = (uintptr_t)sp;

    proc->next = ready_queue_head;
    proc->prev = ready_queue_tail;
    ready_queue_tail->next = proc;
    ready_queue_head->prev = proc;
    ready_queue_tail = proc;

    spin_unlock_irqrestore(&sched_lock, flags);
    return proc;
}

// Find next READY process
struct process* get_next_ready_process(void) {
    if (!current_process) return NULL;
    if (!current_process->next) return current_process;

    struct process* iterator = current_process->next;

    // Scan the full circular list for a READY process.
    while (iterator && iterator != current_process) {
        if (iterator->state == PROCESS_READY) {
            return iterator;
        }
        iterator = iterator->next;
    }
    
    // If current is ready/running, return it.
    if (current_process->state == PROCESS_RUNNING || current_process->state == PROCESS_READY)
        return current_process;
        
    // If EVERYONE is sleeping, we must return the IDLE task (PID 0)
    // Assuming PID 0 is always in the list.
    // Search specifically for PID 0
    iterator = current_process->next;
    while (iterator && iterator->pid != 0) {
        iterator = iterator->next;
        if (iterator == current_process) break; // Should not happen
    }
    return iterator ? iterator : current_process;
}

void schedule(void) {
    uintptr_t irq_flags = spin_lock_irqsave(&sched_lock);

    if (!current_process) {
        spin_unlock_irqrestore(&sched_lock, irq_flags);
        return;
    }

    struct process* prev = current_process;
    struct process* next = get_next_ready_process();
    
    if (prev == next) {
        spin_unlock_irqrestore(&sched_lock, irq_flags);
        return; 
    }

    // Only change state to READY if it was RUNNING. 
    // If it was SLEEPING/BLOCKED, leave it as is.
    if (prev->state == PROCESS_RUNNING) {
        prev->state = PROCESS_READY;
    }

    current_process = next;
    current_process->state = PROCESS_RUNNING;

    if (current_process->addr_space && current_process->addr_space != prev->addr_space) {
        hal_cpu_set_address_space(current_process->addr_space);
    }

    // For ring3->ring0 transitions, esp0 must point to the top of the kernel stack.
    if (current_process->kernel_stack) {
        hal_cpu_set_kernel_stack((uintptr_t)current_process->kernel_stack + 4096);
    }

    spin_unlock_irqrestore(&sched_lock, irq_flags);

    context_switch(&prev->sp, current_process->sp);

    // Do not restore the old IF state after switching stacks.
    // The previous context may have entered schedule() with IF=0 (e.g. syscall/ISR),
    // and propagating that would prevent timer/keyboard IRQs from firing.
    hal_cpu_enable_interrupts();
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
