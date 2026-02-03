#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "uart_console.h"

// Heap starts at 3GB + 256MB
#define KHEAP_START 0xD0000000
#define KHEAP_INITIAL_SIZE (10 * 1024 * 1024) // 10MB
#define PAGE_SIZE 4096

#define HEAP_MAGIC 0xCAFEBABE

// Advanced Header: Doubly Linked List
typedef struct heap_header {
    uint32_t magic;           // Magic number for corruption detection
    size_t size;              // Size of data
    uint8_t is_free;          // 1 = Free, 0 = Used
    struct heap_header* next; // Next block
    struct heap_header* prev; // Previous block
} heap_header_t;

static heap_header_t* head = NULL;
static heap_header_t* tail = NULL;

// Helper to check corruption
void check_integrity(heap_header_t* header) {
    if (header->magic != HEAP_MAGIC) {
        uart_print("\n[HEAP] CRITICAL: Heap Corruption Detected!\n");
        uart_print("Block at: ");
        // TODO: print address
        uart_print(" has invalid magic number.\n");
        for(;;) __asm__("hlt");
    }
}

void kheap_init(void) {
    uart_print("[HEAP] Initializing Advanced Heap (Doubly Linked)...\n");
    
    // ... (rest of mapping code) ...
    // Note: I will need to replace the mapping code block manually or be careful
    // Since I'm using 'edit', let's stick to what changes.
    // I need to find the exact block to replace.
    
    // Actually, I can replace the whole struct definition and init logic.
    // But 'edit' tool replaces EXACT text matches.
    // The previous text block I provided in 'newText' won't match because I added functions.
    
    // Let's abort this 'edit' approach for the whole file and do precise replacements.


void kheap_init(void) {
    uart_print("[HEAP] Initializing Advanced Heap (Doubly Linked)...\n");
    
    // 1. Map pages
    uint32_t pages_needed = KHEAP_INITIAL_SIZE / PAGE_SIZE;
    if (KHEAP_INITIAL_SIZE % PAGE_SIZE != 0) pages_needed++;
    
    uint32_t virt_addr = KHEAP_START;
    
    for (uint32_t i = 0; i < pages_needed; i++) {
        void* phys_frame = pmm_alloc_page();
        if (!phys_frame) {
            uart_print("[HEAP] OOM during init!\n");
            return;
        }
        
        // Map 4KB frame
        vmm_map_page((uint64_t)(uintptr_t)phys_frame, (uint64_t)virt_addr, 
                     VMM_FLAG_PRESENT | VMM_FLAG_RW);
                     
        virt_addr += PAGE_SIZE;
    }
    
    // 2. Initial Block
    head = (heap_header_t*)KHEAP_START;
    head->magic = HEAP_MAGIC; // Set Magic
    head->size = KHEAP_INITIAL_SIZE - sizeof(heap_header_t);
    head->is_free = 1;
    head->next = NULL;
    head->prev = NULL;
    
    tail = head;
    
    uart_print("[HEAP] 10MB Heap Ready.\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align to 8 bytes
    size_t aligned_size = (size + 7) & ~7;
    
    heap_header_t* current = head;
    
    while (current) {
        // Sanity Check
        if (current->magic != HEAP_MAGIC) {
            uart_print("[HEAP] Corruption Detected in kmalloc scan!\n");
            for(;;) __asm__("hlt");
        }

        if (current->is_free && current->size >= aligned_size) {
            // Found candidate. Split?
            if (current->size > aligned_size + sizeof(heap_header_t) + 16) {
                // Create new header in the remaining space
                heap_header_t* new_block = (heap_header_t*)((uint8_t*)current + sizeof(heap_header_t) + aligned_size);
                
                new_block->magic = HEAP_MAGIC; // Set Magic
                new_block->size = current->size - aligned_size - sizeof(heap_header_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                new_block->prev = current;
                
                // Update pointers
                if (current->next) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = aligned_size;
                
                if (current == tail) tail = new_block;
            }
            
            current->is_free = 0;
            return (void*)((uint8_t*)current + sizeof(heap_header_t));
        }
        current = current->next;
    }
    
    uart_print("[HEAP] OOM: kmalloc failed.\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    heap_header_t* header = (heap_header_t*)((uint8_t*)ptr - sizeof(heap_header_t));
    
    if (header->magic != HEAP_MAGIC) {
        uart_print("[HEAP] Corruption Detected in kfree!\n");
        for(;;) __asm__("hlt");
    }

    header->is_free = 1;
    
    // 1. Coalesce Right (Forward)
    if (header->next && header->next->is_free) {
        heap_header_t* next_block = header->next;
        
        header->size += sizeof(heap_header_t) + next_block->size;
        header->next = next_block->next;
        
        if (header->next) {
            header->next->prev = header;
        } else {
            tail = header; // If next was tail, now current is tail
        }
    }
    
    // 2. Coalesce Left (Backward) - The Power of Double Links!
    if (header->prev && header->prev->is_free) {
        heap_header_t* prev_block = header->prev;
        
        prev_block->size += sizeof(heap_header_t) + header->size;
        prev_block->next = header->next;
        
        if (header->next) {
            header->next->prev = prev_block;
        } else {
            tail = prev_block;
        }
        // No need to update 'header' anymore, prev_block is the merged one
    }
}
