#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "idt.h" // For struct registers

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_SLEEPING, // New state
    PROCESS_ZOMBIE
} process_state_t;

struct process {
    uint32_t pid;
    uint32_t esp;
    uint32_t cr3;
    uint32_t* kernel_stack;
    process_state_t state;
    uint32_t wake_at_tick;      // New: When to wake up (global tick count)
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
extern void context_switch(uint32_t* old_esp_ptr, uint32_t new_esp);

// Yield the CPU to the next process voluntarily
void schedule(void);

#endif
