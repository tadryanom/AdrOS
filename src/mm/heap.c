#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "uart_console.h"

// Heap starts at 3GB + 256MB (Arbitrary safe high location)
#define KHEAP_START 0xD0000000
#define KHEAP_INITIAL_SIZE (10 * 1024 * 1024) // 10MB
#define PAGE_SIZE 4096

typedef struct heap_header {
    size_t size;            // Size of the data block (excluding header)
    uint8_t is_free;        // 1 if free, 0 if used
    struct heap_header* next; // Next block in the list
} heap_header_t;

static heap_header_t* head = NULL;

void kheap_init(void) {
    uart_print("[HEAP] Initializing Kernel Heap...\n");
    
    // 1. Map pages for the heap
    // We need to map Virtual Addresses [KHEAP_START] to [KHEAP_START + SIZE]
    // to physical frames.
    
    uint32_t pages_needed = KHEAP_INITIAL_SIZE / PAGE_SIZE;
    if (KHEAP_INITIAL_SIZE % PAGE_SIZE != 0) pages_needed++;
    
    uint32_t virt_addr = KHEAP_START;
    
    for (uint32_t i = 0; i < pages_needed; i++) {
        void* phys_frame = pmm_alloc_page();
        if (!phys_frame) {
            uart_print("[HEAP] OOM during init!\n");
            return;
        }
        
        // Map it!
        // Note: vmm_map_page expects 64-bit phys but we give it 32-bit cast
        vmm_map_page((uint64_t)(uintptr_t)phys_frame, (uint64_t)virt_addr, 
                     VMM_FLAG_PRESENT | VMM_FLAG_RW);
                     
        virt_addr += PAGE_SIZE;
    }
    
    // 2. Create the initial huge free block
    head = (heap_header_t*)KHEAP_START;
    head->size = KHEAP_INITIAL_SIZE - sizeof(heap_header_t);
    head->is_free = 1;
    head->next = NULL;
    
    uart_print("[HEAP] Initialized 10MB at 0xD0000000.\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align size to 8 bytes for performance/safety
    size_t aligned_size = (size + 7) & ~7;
    
    heap_header_t* current = head;
    
    while (current) {
        if (current->is_free && current->size >= aligned_size) {
            // Found a block!
            
            // Can we split it? 
            // Only split if remaining space is big enough for a header + minimal data
            if (current->size > aligned_size + sizeof(heap_header_t) + 8) {
                heap_header_t* new_block = (heap_header_t*)((uint8_t*)current + sizeof(heap_header_t) + aligned_size);
                
                new_block->size = current->size - aligned_size - sizeof(heap_header_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = aligned_size;
                current->next = new_block;
            }
            
            current->is_free = 0;
            return (void*)((uint8_t*)current + sizeof(heap_header_t));
        }
        current = current->next;
    }
    
    uart_print("[HEAP] OOM: No block large enough!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    // Get header
    heap_header_t* header = (heap_header_t*)((uint8_t*)ptr - sizeof(heap_header_t));
    header->is_free = 1;
    
    // Merge with next block if free (Coalescing)
    if (header->next && header->next->is_free) {
        header->size += sizeof(heap_header_t) + header->next->size;
        header->next = header->next->next;
    }
    
    // TODO: We should ideally merge with PREVIOUS block too, 
    // but a singly linked list makes that O(N). 
    // A doubly linked list is better for production heaps.
}
