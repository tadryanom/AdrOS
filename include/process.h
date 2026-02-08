#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "idt.h" // For struct registers
#include "fs.h"

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

struct process {
    uint32_t pid;
    uint32_t parent_pid;
    uintptr_t sp;
    uintptr_t addr_space;
    uint32_t* kernel_stack;
    process_state_t state;
    uint32_t wake_at_tick;      // New: When to wake up (global tick count)
    int exit_status;

    int has_user_regs;
    struct registers user_regs;

    int waiting;
    int wait_pid;
    int wait_result_pid;
    int wait_result_status;
    struct file* files[PROCESS_MAX_FILES];
    struct process* next;
    struct process* prev;       // Doubly linked list helps here too! (Optional but good)
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

// Wait for a child to exit. Returns child's pid on success, -1 on error.
int process_waitpid(int pid, int* status_out);

// Mark current process as exiting and notify/wake a waiter (if any).
void process_exit_notify(int status);

// Create a child process that will resume in usermode from a saved register frame.
struct process* process_fork_create(uintptr_t child_as, const struct registers* child_regs);

#endif
