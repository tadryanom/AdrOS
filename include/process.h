#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "idt.h" // For struct registers

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
} process_state_t;

struct process {
    uint32_t pid;               // Process ID
    uint32_t esp;               // Kernel Stack Pointer (Saved when switched out)
    uint32_t cr3;               // Page Directory (Physical)
    uint32_t* kernel_stack;     // Pointer to the bottom of the allocated kernel stack
    process_state_t state;      // Current state
    struct process* next;       // Linked list for round-robin
};

// Global pointer to the currently running process
extern struct process* current_process;

// Initialize the multitasking system
void process_init(void);

// Create a new kernel thread
struct process* process_create_kernel(void (*entry_point)(void));

// The magic function that switches stacks (Implemented in Assembly)
// old_esp_ptr: Address where we save the OLD process's ESP
// new_esp: The NEW process's ESP to load
extern void context_switch(uint32_t* old_esp_ptr, uint32_t new_esp);

// Yield the CPU to the next process voluntarily
void schedule(void);

#endif
