#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "interrupts.h" // For struct registers
#include "fs.h"
#include "signal.h"

/* clone() flags (Linux-compatible subset) */
#define CLONE_VM        0x00000100  /* Share address space */
#define CLONE_FS        0x00000200  /* Share cwd */
#define CLONE_FILES     0x00000400  /* Share file descriptor table */
#define CLONE_SIGHAND   0x00000800  /* Share signal handlers */
#define CLONE_THREAD    0x00010000  /* Same thread group */
#define CLONE_SETTLS    0x00080000  /* Set TLS for child */
#define CLONE_PARENT_SETTID  0x00100000  /* Store child tid in parent */
#define CLONE_CHILD_CLEARTID 0x00200000  /* Clear child tid on exit */

/* Convenience: flags for a typical pthread_create */
#define CLONE_THREAD_FLAGS  (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SETTLS)

#define PROCESS_FLAG_THREAD  0x01  /* This process is a thread (not group leader) */

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_SLEEPING, // New state
    PROCESS_ZOMBIE
} process_state_t;

struct file {
    fs_node_t* node;
    uint32_t offset;
    uint32_t flags;
    uint32_t refcount;
};

#define PROCESS_MAX_FILES 16

#define PROCESS_MAX_SIG 32

struct process {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t session_id;
    uint32_t pgrp_id;
    uint32_t uid;
    uint32_t gid;
    uintptr_t sp;
    uintptr_t addr_space;
    uint32_t* kernel_stack;
#define SCHED_NUM_PRIOS 32
#define SCHED_DEFAULT_PRIO 16

    uint8_t priority;           // 0 = highest, 31 = lowest
    int8_t  nice;               // -20 to +19 (maps to priority)
    process_state_t state;
    uint32_t wake_at_tick;
    uint32_t alarm_tick;
    uint32_t utime;             /* ticks spent in user mode */
    uint32_t stime;             /* ticks spent in kernel mode */
    int exit_status;

    int has_user_regs;
    struct registers user_regs;

    // Minimal signals: per-signal action, blocked mask and pending mask.
    // sa_handler == 0 => default
    // sa_handler == 1 => ignore
    // sa_handler >= 2 => user handler address
    struct sigaction sigactions[PROCESS_MAX_SIG];
    uint32_t sig_blocked_mask;
    uint32_t sig_pending_mask;

    // For SIGSEGV: last page fault address (CR2) captured in ring3.
    uintptr_t last_fault_addr;

#define PROCESS_MAX_MMAPS 32
    struct {
        uintptr_t base;
        uint32_t  length;
        int       shmid;       /* shm segment id, or -1 if not shm */
    } mmaps[PROCESS_MAX_MMAPS];

    uintptr_t heap_start;
    uintptr_t heap_break;

    char cwd[128];
    uint32_t umask;

    int waiting;
    int wait_pid;
    int wait_result_pid;
    int wait_result_status;
    struct file* files[PROCESS_MAX_FILES];
    uint8_t fd_flags[PROCESS_MAX_FILES];
    struct process* next;
    struct process* prev;

    struct process* rq_next;    // O(1) runqueue per-priority list
    struct process* rq_prev;

    /* Thread support */
    uint32_t tgid;              /* Thread group ID (== pid for group leader) */
    uint32_t flags;             /* PROCESS_FLAG_* */
    uintptr_t tls_base;         /* User-space TLS base (set via SET_THREAD_AREA) */
    uint32_t* clear_child_tid;  /* User address to clear + futex-wake on exit */
};

// Global pointer to the currently running process
extern struct process* current_process;

// Initialize the multitasking system
void process_init(void);

// Create a new kernel thread
struct process* process_create_kernel(void (*entry_point)(void));

// Sleep for N ticks
void process_sleep(uint32_t ticks);

// Wake up sleeping processes (called by timer)
void process_wake_check(uint32_t current_tick);

// The magic function that switches stacks (Implemented in Assembly)
// old_esp_ptr: Address where we save the OLD process's ESP
// new_esp: The NEW process's ESP to load
extern void context_switch(uintptr_t* old_sp_ptr, uintptr_t new_sp);

// Yield the CPU to the next process voluntarily
void schedule(void);

// Wait for a child to exit. Returns child's pid on success, 0 on WNOHANG no-status, -1 on error.
int process_waitpid(int pid, int* status_out, uint32_t options);

// Mark current process as exiting and notify/wake a waiter (if any).
void process_exit_notify(int status);

// Enqueue a READY process into the active O(1) runqueue.
// Must be called whenever a process transitions to PROCESS_READY from outside scheduler.c.
void sched_enqueue_ready(struct process* p);

// Kill a process (minimal signals). Returns 0 on success or -errno.
int process_kill(uint32_t pid, int sig);

// Send a signal to all processes in a process group.
int process_kill_pgrp(uint32_t pgrp, int sig);

// Create a child process that will resume in usermode from a saved register frame.
struct process* process_fork_create(uintptr_t child_as, const struct registers* child_regs);

// Create a thread (clone) sharing the parent's address space.
struct process* process_clone_create(uint32_t clone_flags,
                                     uintptr_t child_stack,
                                     const struct registers* child_regs,
                                     uintptr_t tls_base);

// Look up a process by PID (scheduler lock must NOT be held).
struct process* process_find_by_pid(uint32_t pid);

#endif
