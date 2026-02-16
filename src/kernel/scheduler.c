#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "console.h"
#include "timer.h" // Need access to current tick usually, but we pass it in wake_check
#include "spinlock.h"
#include "utils.h"
#include "errno.h"
#include "hal/cpu.h"
#include "hal/usermode.h"
#include "arch_process.h"
#include "arch_fpu.h"
#include "sched_pcpu.h"
#include <stddef.h>

#ifndef __i386__
struct process* current_process = NULL;
#endif
struct process* ready_queue_head = NULL;
struct process* ready_queue_tail = NULL;
static uint32_t next_pid = 1;

static spinlock_t sched_lock = {0};
static uintptr_t kernel_as = 0;

/*
 * Kernel stack allocator with guard pages.
 * Layout per slot: [guard page (unmapped)] [2 stack pages (mapped)]
 * Virtual region: 0xC8000000 .. 0xCFFFFFFF (128MB, up to 10922 stacks)
 */
#define KSTACK_REGION  0xC8000000U
#define KSTACK_PAGES   2              /* 8KB usable stack per thread */
#define KSTACK_SIZE    (KSTACK_PAGES * 0x1000U)
#define KSTACK_SLOT    (0x1000U + KSTACK_SIZE)  /* guard + stack */
#define KSTACK_MAX     10922

static uint32_t kstack_next_slot = 0;
static spinlock_t kstack_lock = {0};

static void* kstack_alloc(void) {
    uintptr_t flags = spin_lock_irqsave(&kstack_lock);
    if (kstack_next_slot >= KSTACK_MAX) {
        spin_unlock_irqrestore(&kstack_lock, flags);
        return NULL;
    }
    uint32_t slot = kstack_next_slot++;
    spin_unlock_irqrestore(&kstack_lock, flags);

    uintptr_t base = KSTACK_REGION + slot * KSTACK_SLOT;
    /* base+0x0000 = guard page (leave unmapped) */
    /* base+0x1000 .. base+0x1000+KSTACK_SIZE = actual stack pages */
    for (uint32_t i = 0; i < KSTACK_PAGES; i++) {
        void* phys = pmm_alloc_page();
        if (!phys) return NULL;
        vmm_map_page((uint64_t)(uintptr_t)phys,
                     (uint64_t)(base + 0x1000U + i * 0x1000U),
                     VMM_FLAG_PRESENT | VMM_FLAG_RW);
    }
    memset((void*)(base + 0x1000U), 0, KSTACK_SIZE);
    return (void*)(base + 0x1000U);
}

static void kstack_free(void* stack) {
    if (!stack) return;
    uintptr_t addr = (uintptr_t)stack;
    if (addr < KSTACK_REGION || addr >= KSTACK_REGION + KSTACK_MAX * KSTACK_SLOT)
        return;
    for (uint32_t i = 0; i < KSTACK_PAGES; i++)
        vmm_unmap_page((uint64_t)(addr + i * 0x1000U));
    /* Note: slot is not recycled — acceptable for now */
}

/* ---------- O(1) runqueue ---------- */
struct prio_queue {
    struct process* head;
    struct process* tail;
};

struct runqueue {
    uint32_t bitmap;                        // bit i set => queue[i] non-empty
    struct prio_queue queue[SCHED_NUM_PRIOS];
};

/* Per-CPU runqueue: each CPU has its own active/expired pair + idle process */
struct cpu_rq {
    struct runqueue stores[2];
    struct runqueue *active;
    struct runqueue *expired;
    struct process  *idle;       /* per-CPU idle (PID 0 on BSP) */
};

#ifdef __i386__
#include "arch/x86/smp.h"
#define SCHED_MAX_CPUS SMP_MAX_CPUS
#else
#define SCHED_MAX_CPUS 1
#endif

static struct cpu_rq pcpu_rq[SCHED_MAX_CPUS];

static inline uint32_t bsf32(uint32_t v) {
    return (uint32_t)__builtin_ctz(v);
}

static void rq_enqueue(struct runqueue* rq, struct process* p) {
    uint8_t prio = p->priority;
    struct prio_queue* pq = &rq->queue[prio];
    p->rq_next = NULL;
    p->rq_prev = pq->tail;
    if (pq->tail) pq->tail->rq_next = p;
    else          pq->head = p;
    pq->tail = p;
    rq->bitmap |= (1U << prio);
}

static void rq_dequeue(struct runqueue* rq, struct process* p) {
    uint8_t prio = p->priority;
    struct prio_queue* pq = &rq->queue[prio];
    if (p->rq_prev) p->rq_prev->rq_next = p->rq_next;
    else            pq->head = p->rq_next;
    if (p->rq_next) p->rq_next->rq_prev = p->rq_prev;
    else            pq->tail = p->rq_prev;
    p->rq_next = NULL;
    p->rq_prev = NULL;
    if (!pq->head) rq->bitmap &= ~(1U << prio);
}

static void rq_remove_if_queued(struct process* p) {
    uint8_t prio = p->priority;
    struct process* it;
    struct cpu_rq *crq = &pcpu_rq[p->cpu_id < SCHED_MAX_CPUS ? p->cpu_id : 0];

    it = crq->active->queue[prio].head;
    while (it) {
        if (it == p) { rq_dequeue(crq->active, p); return; }
        it = it->rq_next;
    }

    it = crq->expired->queue[prio].head;
    while (it) {
        if (it == p) { rq_dequeue(crq->expired, p); return; }
        it = it->rq_next;
    }
}

/* ---------- Sorted sleep queue (by wake_at_tick) ---------- */
static struct process* sleep_head = NULL;

static void sleep_queue_insert(struct process* p) {
    p->in_sleep_queue = 1;
    if (!sleep_head || p->wake_at_tick <= sleep_head->wake_at_tick) {
        p->sleep_prev = NULL;
        p->sleep_next = sleep_head;
        if (sleep_head) sleep_head->sleep_prev = p;
        sleep_head = p;
        return;
    }
    struct process* cur = sleep_head;
    while (cur->sleep_next && cur->sleep_next->wake_at_tick < p->wake_at_tick)
        cur = cur->sleep_next;
    p->sleep_next = cur->sleep_next;
    p->sleep_prev = cur;
    if (cur->sleep_next) cur->sleep_next->sleep_prev = p;
    cur->sleep_next = p;
}

static void sleep_queue_remove(struct process* p) {
    if (!p->in_sleep_queue) return;
    if (p->sleep_prev) p->sleep_prev->sleep_next = p->sleep_next;
    else               sleep_head = p->sleep_next;
    if (p->sleep_next) p->sleep_next->sleep_prev = p->sleep_prev;
    p->sleep_prev = NULL;
    p->sleep_next = NULL;
    p->in_sleep_queue = 0;
}

/* ---------- Sorted alarm queue (by alarm_tick) ---------- */
static struct process* alarm_head = NULL;

static void alarm_queue_insert(struct process* p) {
    p->in_alarm_queue = 1;
    if (!alarm_head || p->alarm_tick <= alarm_head->alarm_tick) {
        p->alarm_prev = NULL;
        p->alarm_next = alarm_head;
        if (alarm_head) alarm_head->alarm_prev = p;
        alarm_head = p;
        return;
    }
    struct process* cur = alarm_head;
    while (cur->alarm_next && cur->alarm_next->alarm_tick < p->alarm_tick)
        cur = cur->alarm_next;
    p->alarm_next = cur->alarm_next;
    p->alarm_prev = cur;
    if (cur->alarm_next) cur->alarm_next->alarm_prev = p;
    cur->alarm_next = p;
}

static void alarm_queue_remove(struct process* p) {
    if (!p->in_alarm_queue) return;
    if (p->alarm_prev) p->alarm_prev->alarm_next = p->alarm_next;
    else               alarm_head = p->alarm_next;
    if (p->alarm_next) p->alarm_next->alarm_prev = p->alarm_prev;
    p->alarm_prev = NULL;
    p->alarm_next = NULL;
    p->in_alarm_queue = 0;
}

static struct process* rq_pick_next(uint32_t cpu) {
    struct cpu_rq *crq = &pcpu_rq[cpu];
    if (crq->active->bitmap) {
        uint32_t prio = bsf32(crq->active->bitmap);
        return crq->active->queue[prio].head;
    }
    // Swap active <-> expired
    struct runqueue* tmp = crq->active;
    crq->active = crq->expired;
    crq->expired = tmp;
    if (crq->active->bitmap) {
        uint32_t prio = bsf32(crq->active->bitmap);
        return crq->active->queue[prio].head;
    }
    return NULL;  // only idle task left
}

void sched_enqueue_ready(struct process* p) {
    if (!p) return;
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    sleep_queue_remove(p);
    if (p->state == PROCESS_READY) {
        uint32_t cpu = p->cpu_id < SCHED_MAX_CPUS ? p->cpu_id : 0;
        rq_enqueue(pcpu_rq[cpu].active, p);
        sched_pcpu_inc_load(cpu);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
}

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

    /* Safety net: ensure process is not in any runqueue/sleep queue before freeing */
    rq_remove_if_queued(p);
    sleep_queue_remove(p);

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
        kstack_free(p->kernel_stack);
        p->kernel_stack = NULL;
    }

    if (p->addr_space && p->addr_space != kernel_as) {
        /* Threads share addr_space with group leader; don't destroy it */
        if (!(p->flags & PROCESS_FLAG_THREAD)) {
            vmm_as_destroy(p->addr_space);
        }
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

        if (__sync_sub_and_fetch(&f->refcount, 1) == 0) {
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
        /* Remove from runqueue/sleep queue BEFORE marking ZOMBIE */
        if (p->state == PROCESS_READY) {
            rq_remove_if_queued(p);
        }
        sleep_queue_remove(p);
        alarm_queue_remove(p);
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
                    rq_enqueue(pcpu_rq[parent->cpu_id].active, parent);
                }
            }
        }
    } else {
        p->sig_pending_mask |= (1U << (uint32_t)sig);
        if (p->state == PROCESS_BLOCKED || p->state == PROCESS_SLEEPING) {
            sleep_queue_remove(p);
            p->state = PROCESS_READY;
            rq_enqueue(pcpu_rq[p->cpu_id].active, p);
        }
    }

    spin_unlock_irqrestore(&sched_lock, flags);
    return 0;
}

int process_kill_pgrp(uint32_t pgrp, int sig) {
    if (pgrp == 0) return -EINVAL;
    if (sig <= 0 || sig >= PROCESS_MAX_SIG) return -EINVAL;

    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    int found = 0;

    struct process* it = ready_queue_head;
    if (it) {
        const struct process* const start = it;
        do {
            if (it->pgrp_id == pgrp && it->pid != 0 && it->state != PROCESS_ZOMBIE) {
                it->sig_pending_mask |= (1U << (uint32_t)sig);
                if (it->state == PROCESS_BLOCKED || it->state == PROCESS_SLEEPING) {
                    sleep_queue_remove(it);
                    it->state = PROCESS_READY;
                    rq_enqueue(pcpu_rq[it->cpu_id].active, it);
                }
                found = 1;
            }
            it = it->next;
        } while (it && it != start);
    }

    spin_unlock_irqrestore(&sched_lock, flags);
    return found ? 0 : -ESRCH;
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
            } while (it && it != start);
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
    alarm_queue_remove(current_process);

    if (current_process->pid != 0) {
        struct process* parent = process_find_locked(current_process->parent_pid);
        if (parent && parent->state == PROCESS_BLOCKED && parent->waiting) {
            if (parent->wait_pid == -1 || parent->wait_pid == (int)current_process->pid) {
                parent->wait_result_pid = (int)current_process->pid;
                parent->wait_result_status = status;
                parent->state = PROCESS_READY;
                rq_enqueue(pcpu_rq[parent->cpu_id].active, parent);
            }
        }
    }

    spin_unlock_irqrestore(&sched_lock, flags);
}

static void fork_child_trampoline(void) {
    if (!current_process || !current_process->has_user_regs) {
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    if (current_process->addr_space) {
        vmm_as_activate(current_process->addr_space);
    }

    hal_usermode_enter_regs(current_process->user_regs);
}

struct process* process_fork_create(uintptr_t child_as, const void* child_regs) {
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
    proc->uid = current_process ? current_process->uid : 0;
    proc->gid = current_process ? current_process->gid : 0;
    proc->euid = current_process ? current_process->euid : 0;
    proc->egid = current_process ? current_process->egid : 0;
    proc->priority = current_process ? current_process->priority : SCHED_DEFAULT_PRIO;
    proc->nice = current_process ? current_process->nice : 0;
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
    memcpy(proc->user_regs, child_regs, ARCH_REGS_SIZE);
    proc->tgid = proc->pid;
    proc->flags = 0;
    proc->tls_base = 0;
    proc->clear_child_tid = NULL;
    proc->cpu_id = 0;

    if (current_process) {
        memcpy(proc->fpu_state, current_process->fpu_state, FPU_STATE_SIZE);
    } else {
        arch_fpu_init_state(proc->fpu_state);
    }

    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        proc->files[i] = NULL;
    }
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        proc->mmaps[i] = current_process ? current_process->mmaps[i]
                                         : (typeof(proc->mmaps[i])){0, 0, -1};
    }

    void* stack = kstack_alloc();
    if (!stack) {
        kfree(proc);
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }
    proc->kernel_stack = (uint32_t*)stack;

    proc->sp = arch_kstack_init((uint8_t*)stack + KSTACK_SIZE,
                                  thread_wrapper, fork_child_trampoline);

    proc->next = ready_queue_head;
    proc->prev = ready_queue_tail;
    ready_queue_tail->next = proc;
    ready_queue_head->prev = proc;
    ready_queue_tail = proc;

    rq_enqueue(pcpu_rq[proc->cpu_id].active, proc);

    spin_unlock_irqrestore(&sched_lock, flags);
    return proc;
}

static void clone_child_trampoline(void) {
    if (!current_process || !current_process->has_user_regs) {
        process_exit_notify(1);
        schedule();
        for (;;) hal_cpu_idle();
    }

    /* Activate the shared address space */
    if (current_process->addr_space) {
        vmm_as_activate(current_process->addr_space);
    }

    /* Load user TLS into GS if set */
    if (current_process->tls_base) {
        hal_cpu_set_tls(current_process->tls_base);
    }

    hal_usermode_enter_regs(current_process->user_regs);
}

struct process* process_clone_create(uint32_t clone_flags,
                                     uintptr_t child_stack,
                                     const void* child_regs,
                                     uintptr_t tls_base) {
    if (!child_regs || !current_process) return NULL;

    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    struct process* proc = (struct process*)kmalloc(sizeof(*proc));
    if (!proc) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }
    memset(proc, 0, sizeof(*proc));

    proc->pid = next_pid++;
    proc->parent_pid = current_process->pid;
    proc->session_id = current_process->session_id;
    proc->pgrp_id = current_process->pgrp_id;
    proc->priority = current_process->priority;
    proc->nice = current_process->nice;
    proc->state = PROCESS_READY;
    proc->wake_at_tick = 0;
    proc->exit_status = 0;
    proc->waiting = 0;
    proc->wait_pid = -1;
    proc->wait_result_pid = -1;
    proc->wait_result_status = 0;

    /* CLONE_VM: share address space */
    if (clone_flags & CLONE_VM) {
        proc->addr_space = current_process->addr_space;
        proc->flags |= PROCESS_FLAG_THREAD;
    } else {
        proc->addr_space = vmm_as_clone_user_cow(current_process->addr_space);
        if (!proc->addr_space) {
            kfree(proc);
            spin_unlock_irqrestore(&sched_lock, flags);
            return NULL;
        }
    }

    /* CLONE_THREAD: same thread group */
    if (clone_flags & CLONE_THREAD) {
        proc->tgid = current_process->tgid;
    } else {
        proc->tgid = proc->pid;
    }

    /* CLONE_FS: share cwd */
    strcpy(proc->cwd, current_process->cwd);

    /* CLONE_FILES: share file descriptor table */
    if (clone_flags & CLONE_FILES) {
        for (int i = 0; i < PROCESS_MAX_FILES; i++) {
            proc->files[i] = current_process->files[i];
            if (proc->files[i]) {
                __sync_fetch_and_add(&proc->files[i]->refcount, 1);
            }
            proc->fd_flags[i] = current_process->fd_flags[i];
        }
    }

    /* CLONE_SIGHAND: share signal handlers */
    if (clone_flags & CLONE_SIGHAND) {
        for (int i = 0; i < PROCESS_MAX_SIG; i++) {
            proc->sigactions[i] = current_process->sigactions[i];
        }
    }

    /* CLONE_SETTLS: set TLS base */
    if (clone_flags & CLONE_SETTLS) {
        proc->tls_base = tls_base;
    }

    proc->uid = current_process->uid;
    proc->gid = current_process->gid;
    proc->euid = current_process->euid;
    proc->egid = current_process->egid;
    proc->heap_start = current_process->heap_start;
    proc->heap_break = current_process->heap_break;

    memcpy(proc->fpu_state, current_process->fpu_state, FPU_STATE_SIZE);

    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        proc->mmaps[i] = current_process->mmaps[i];
    }

    proc->has_user_regs = 1;
    memcpy(proc->user_regs, child_regs, ARCH_REGS_SIZE);
    arch_regs_set_retval(proc->user_regs, 0); /* child returns 0 */

    /* If child_stack specified, override user stack pointer */
    if (child_stack) {
        arch_regs_set_ustack(proc->user_regs, child_stack);
    }

    /* Allocate kernel stack */
    void* kstack = kstack_alloc();
    if (!kstack) {
        if (!(clone_flags & CLONE_VM) && proc->addr_space) {
            vmm_as_destroy(proc->addr_space);
        }
        kfree(proc);
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }
    proc->kernel_stack = (uint32_t*)kstack;

    proc->sp = arch_kstack_init((uint8_t*)kstack + KSTACK_SIZE,
                                  thread_wrapper, clone_child_trampoline);

    proc->cpu_id = 0;

    /* Insert into process list */
    proc->next = ready_queue_head;
    proc->prev = ready_queue_tail;
    ready_queue_tail->next = proc;
    ready_queue_head->prev = proc;
    ready_queue_tail = proc;

    rq_enqueue(pcpu_rq[proc->cpu_id].active, proc);

    spin_unlock_irqrestore(&sched_lock, flags);
    return proc;
}

struct process* process_find_by_pid(uint32_t pid) {
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    struct process* p = process_find_locked(pid);
    spin_unlock_irqrestore(&sched_lock, flags);
    return p;
}

void process_init(void) {
    kprintf("[SCHED] Initializing Multitasking...\n");

    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    // Initial Kernel Thread (PID 0) - IDLE TASK
    struct process* kernel_proc = (struct process*)kmalloc(sizeof(*kernel_proc));
    if (!kernel_proc) {
        spin_unlock_irqrestore(&sched_lock, flags);
        kprintf("[SCHED] OOM allocating kernel process struct.\n");
        for(;;) hal_cpu_idle();
        __builtin_unreachable();
    }

    memset(kernel_proc, 0, sizeof(*kernel_proc));

    /* Initialize per-CPU runqueues */
    for (uint32_t c = 0; c < SCHED_MAX_CPUS; c++) {
        memset(&pcpu_rq[c], 0, sizeof(pcpu_rq[c]));
        pcpu_rq[c].active  = &pcpu_rq[c].stores[0];
        pcpu_rq[c].expired = &pcpu_rq[c].stores[1];
        pcpu_rq[c].idle    = NULL;
    }

    kernel_proc->pid = 0;
    kernel_proc->parent_pid = 0;
    kernel_proc->session_id = 0;
    kernel_proc->pgrp_id = 0;
    kernel_proc->priority = SCHED_NUM_PRIOS - 1;  // idle = lowest priority
    kernel_proc->nice = 19;
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
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        kernel_proc->mmaps[i].shmid = -1;
    }
    
    kernel_proc->tgid = 0;
    kernel_proc->flags = 0;
    kernel_proc->tls_base = 0;
    kernel_proc->clear_child_tid = NULL;
    kernel_proc->cpu_id = 0;

    arch_fpu_init_state(kernel_proc->fpu_state);

    /* Allocate a dedicated kernel stack for PID 0 with guard page. */
    void* kstack0 = kstack_alloc();
    if (!kstack0) {
        spin_unlock_irqrestore(&sched_lock, flags);
        kprintf("[SCHED] OOM allocating PID 0 kernel stack.\n");
        for (;;) hal_cpu_idle();
        __builtin_unreachable();
    }
    kernel_proc->kernel_stack = (uint32_t*)kstack0;

    pcpu_rq[0].idle = kernel_proc;
    percpu_set_current(kernel_proc);
    ready_queue_head = kernel_proc;
    ready_queue_tail = kernel_proc;
    kernel_proc->next = kernel_proc;
    kernel_proc->prev = kernel_proc;

    hal_cpu_set_kernel_stack((uintptr_t)kstack0 + KSTACK_SIZE);

    spin_unlock_irqrestore(&sched_lock, flags);
}

void sched_ap_init(uint32_t cpu) {
    if (cpu == 0 || cpu >= SCHED_MAX_CPUS) return;

    /* Allocate OUTSIDE sched_lock to avoid ABBA deadlock with heap lock:
     * AP: sched_lock → heap_lock (kmalloc)
     * BSP: heap_lock → timer ISR → sched_lock  ← deadlock */
    struct process* idle = (struct process*)kmalloc(sizeof(*idle));
    if (!idle) {
        kprintf("[SCHED] CPU%u: OOM allocating idle process.\n", cpu);
        return;
    }
    memset(idle, 0, sizeof(*idle));

    void* kstack = kstack_alloc();
    if (!kstack) {
        kfree(idle);
        kprintf("[SCHED] CPU%u: OOM allocating idle kstack.\n", cpu);
        return;
    }

    /* Fill in idle process fields (no lock needed — not yet visible) */
    idle->parent_pid = 0;
    idle->priority = SCHED_NUM_PRIOS - 1;
    idle->nice = 19;
    idle->state = PROCESS_RUNNING;
    idle->addr_space = kernel_as;
    idle->cpu_id = cpu;
    strcpy(idle->cwd, "/");
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++)
        idle->mmaps[i].shmid = -1;
    idle->kernel_stack = (uint32_t*)kstack;
    arch_fpu_init_state(idle->fpu_state);

    /* Take sched_lock only for PID assignment + list insertion */
    uintptr_t flags = spin_lock_irqsave(&sched_lock);

    idle->pid = next_pid++;
    idle->tgid = idle->pid;

    /* Insert into global process list */
    idle->next = ready_queue_head;
    idle->prev = ready_queue_tail;
    ready_queue_tail->next = idle;
    ready_queue_head->prev = idle;
    ready_queue_tail = idle;

    /* Register as this CPU's idle process and current process */
    pcpu_rq[cpu].idle = idle;
    percpu_set_current(idle);

    spin_unlock_irqrestore(&sched_lock, flags);

    kprintf("[SCHED] CPU%u idle process PID %u ready.\n", cpu, idle->pid);
}

extern spinlock_t sched_lock;

void thread_wrapper(void (*fn)(void)) {
    /* We arrive here from context_switch while schedule() still holds
     * sched_lock with interrupts disabled.  Release the lock and
     * enable interrupts so the new thread can run normally. */
    spin_unlock(&sched_lock);
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
    proc->priority = SCHED_DEFAULT_PRIO;
    proc->nice = 0;
    proc->state = PROCESS_READY;
    proc->addr_space = kernel_as ? kernel_as : (current_process ? current_process->addr_space : 0);
    proc->wake_at_tick = 0;
    proc->exit_status = 0;
    proc->waiting = 0;
    proc->wait_pid = -1;
    proc->wait_result_pid = -1;
    proc->wait_result_status = 0;
    proc->tgid = proc->pid;
    proc->flags = 0;
    proc->tls_base = 0;
    proc->clear_child_tid = NULL;
    proc->cpu_id = 0;

    arch_fpu_init_state(proc->fpu_state);

    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        proc->files[i] = NULL;
    }
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        proc->mmaps[i].shmid = -1;
    }
    
    void* stack = kstack_alloc();
    if (!stack) {
        kfree(proc);
        spin_unlock_irqrestore(&sched_lock, flags);
        return NULL;
    }

    proc->kernel_stack = (uint32_t*)stack;
    
    proc->sp = arch_kstack_init((uint8_t*)stack + KSTACK_SIZE,
                                  thread_wrapper, entry_point);

    proc->next = ready_queue_head;
    proc->prev = ready_queue_tail;
    ready_queue_tail->next = proc;
    ready_queue_head->prev = proc;
    ready_queue_tail = proc;

    rq_enqueue(pcpu_rq[proc->cpu_id].active, proc);

    spin_unlock_irqrestore(&sched_lock, flags);
    return proc;
}

void schedule(void) {
    uintptr_t irq_flags = spin_lock_irqsave(&sched_lock);

    if (!current_process) {
        spin_unlock_irqrestore(&sched_lock, irq_flags);
        return;
    }

    uint32_t cpu = percpu_cpu_index();
    struct cpu_rq *crq = &pcpu_rq[cpu];
    struct process* prev = current_process;

    // Time-slice preemption: if the process is still running (timer
    // preemption, not a voluntary yield) and has quantum left, do NOT
    // preempt.  Woken processes accumulate in active and get their
    // turn when the slice expires.  This limits context-switch rate to
    // TIMER_HZ/SCHED_TIME_SLICE while keeping full tick resolution for
    // sleep/wake timing.
    if (prev->state == PROCESS_RUNNING) {
        if (prev->time_slice > 0) {
            prev->time_slice--;
            spin_unlock_irqrestore(&sched_lock, irq_flags);
            return;
        }
        // Slice exhausted — enqueue to expired with priority decay.
        prev->state = PROCESS_READY;
        if (prev->priority < SCHED_NUM_PRIOS - 1) prev->priority++;
        rq_enqueue(crq->expired, prev);
    } else if (prev->state == PROCESS_SLEEPING && !prev->in_sleep_queue) {
        /* Deferred sleep queue insertion: the caller set SLEEPING + wake_at_tick
         * under its own lock (e.g. semaphore), then called schedule().
         * We insert here under sched_lock — no preemption window. */
        sleep_queue_remove(prev); /* defensive */
        sleep_queue_insert(prev);
    }

    // Pick highest-priority READY process from this CPU's runqueues (O(1) bitmap).
    // rq_pick_next() may swap active/expired internally.
    struct process* next = rq_pick_next(cpu);

    if (next) {
        // next came from active — safe to dequeue.
        rq_dequeue(crq->active, next);
        sched_pcpu_dec_load(cpu);
    } else {
        // Nothing in runqueues.
        if (prev->state == PROCESS_READY) {
            // prev was just enqueued to expired — pull it back.
            rq_dequeue(crq->expired, prev);
            next = prev;
        } else {
            // Fall back to this CPU's idle process.
            if (crq->idle) {
                next = crq->idle;
            } else {
                // Legacy fallback: find PID 0 in process list.
                struct process* it = ready_queue_head;
                next = it;
                if (it) {
                    const struct process* start = it;
                    do {
                        if (it->pid == 0) { next = it; break; }
                        it = it->next;
                    } while (it && it != start);
                }
            }
        }
    }

    if (prev == next) {
        prev->state = PROCESS_RUNNING;
        prev->time_slice = SCHED_TIME_SLICE;
        spin_unlock_irqrestore(&sched_lock, irq_flags);
        return;
    }

    percpu_set_current(next);
    current_process->state = PROCESS_RUNNING;
    current_process->time_slice = SCHED_TIME_SLICE;

    if (current_process->addr_space && current_process->addr_space != prev->addr_space) {
        hal_cpu_set_address_space(current_process->addr_space);
    }

    /* Only update TSS kernel stack on CPU 0 — the TSS is shared and
     * only the BSP runs user processes that need ring 0 stack in TSS. */
    if (cpu == 0 && current_process->kernel_stack) {
        hal_cpu_set_kernel_stack((uintptr_t)current_process->kernel_stack + KSTACK_SIZE);
    }

    /* context_switch MUST execute with the lock held and interrupts
     * disabled.  Otherwise a timer firing between unlock and
     * context_switch would call schedule() again while current_process
     * is already set to `next` but we're still on prev's stack,
     * corrupting next->sp.
     *
     * After context_switch we are on the new process's stack.
     * irq_flags now holds the value saved during THIS process's
     * previous spin_lock_irqsave in schedule().  Releasing the
     * lock restores the correct interrupt state.
     *
     * For brand-new processes, context_switch's `ret` goes to
     * thread_wrapper which releases the lock explicitly. */
    arch_fpu_save(prev->fpu_state);
    context_switch(&prev->sp, current_process->sp);
    arch_fpu_restore(current_process->fpu_state);

    spin_unlock_irqrestore(&sched_lock, irq_flags);
}

void sched_sleep_enqueue_self(void) {
    /* No-op: schedule() now handles deferred sleep queue insertion
     * atomically under sched_lock, eliminating preemption windows. */
}

void process_sleep(uint32_t ticks) {
    extern uint32_t get_tick_count(void);
    uint32_t current_tick = get_tick_count();

    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    current_process->wake_at_tick = current_tick + ticks;
    current_process->state = PROCESS_SLEEPING;
    sleep_queue_remove(current_process); /* defensive: remove stale entry if present */
    sleep_queue_insert(current_process);
    spin_unlock_irqrestore(&sched_lock, flags);

    schedule();
}

void process_wake_check(uint32_t current_tick) {
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    struct process* iter = ready_queue_head;

    if (!iter) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return;
    }

    /* CPU time accounting: charge one tick to the running process */
    if (current_process && current_process->state == PROCESS_RUNNING) {
        current_process->utime++;
    }

    /* O(1) sleep queue: pop expired entries from the sorted head */
    while (sleep_head && current_tick >= sleep_head->wake_at_tick) {
        struct process* p = sleep_head;
        sleep_queue_remove(p);
        if (p->state == PROCESS_SLEEPING) {
            p->state = PROCESS_READY;
            if (p->priority > 0) p->priority--;
            rq_enqueue(pcpu_rq[p->cpu_id].active, p);
        }
    }

    /* O(1) alarm queue: pop expired entries from the sorted head */
    while (alarm_head && current_tick >= alarm_head->alarm_tick) {
        struct process* p = alarm_head;
        alarm_queue_remove(p);
        p->sig_pending_mask |= (1U << 14); /* SIGALRM */
        /* Re-arm repeating ITIMER_REAL */
        if (p->alarm_interval != 0) {
            p->alarm_tick = current_tick + p->alarm_interval;
            alarm_queue_insert(p);
        } else {
            p->alarm_tick = 0;
        }
    }

    /* ITIMER_VIRTUAL: decrement when running in user mode */
    if (current_process && current_process->state == PROCESS_RUNNING) {
        if (current_process->itimer_virt_value > 0) {
            current_process->itimer_virt_value--;
            if (current_process->itimer_virt_value == 0) {
                current_process->sig_pending_mask |= (1U << 26); /* SIGVTALRM */
                current_process->itimer_virt_value = current_process->itimer_virt_interval;
            }
        }
        /* ITIMER_PROF: decrement when running (user + kernel) */
        if (current_process->itimer_prof_value > 0) {
            current_process->itimer_prof_value--;
            if (current_process->itimer_prof_value == 0) {
                current_process->sig_pending_mask |= (1U << 27); /* SIGPROF */
                current_process->itimer_prof_value = current_process->itimer_prof_interval;
            }
        }
    }

    spin_unlock_irqrestore(&sched_lock, flags);
}

uint32_t process_alarm_set(struct process* p, uint32_t tick) {
    uintptr_t flags = spin_lock_irqsave(&sched_lock);
    uint32_t old = p->alarm_tick;

    /* Remove from alarm queue if currently queued */
    alarm_queue_remove(p);

    if (tick != 0) {
        p->alarm_tick = tick;
        alarm_queue_insert(p);
    } else {
        p->alarm_tick = 0;
    }

    spin_unlock_irqrestore(&sched_lock, flags);
    return old;
}
