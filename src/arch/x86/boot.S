/*
 * AdrOS - x86 Higher Half Kernel Bootstrap
 */

/* Constants */
.set MB_MAGIC, 0xE85250D6
.set MB_ARCH,  0          /* i386 */
.set KERNEL_VIRT_BASE, 0xC0000000
.set PAGE_SIZE, 4096

/* 
 * This macro converts a virtual address (symbol) to a physical address 
 * by subtracting 3GB. We need this before paging is enabled.
 */
#define V2P(x) ((x) - KERNEL_VIRT_BASE)

.section .multiboot_header
.align 8
multiboot_header_start:
    .long MB_MAGIC
    .long MB_ARCH
    .long multiboot_header_end - multiboot_header_start
    .long 0x100000000 - (MB_MAGIC + MB_ARCH + (multiboot_header_end - multiboot_header_start))
    .word 0, 0
    .long 8
multiboot_header_end:

.section .text
.global _start
.type _start, @function

_start:
    /* 
     * We are loaded at 1MB physical. Paging is OFF.
     * CPU is executing instructions here.
     * ESP is unknown.
     */
    
    /* Setup a temporary stack (using physical address) */
    mov $(V2P(stack_top)), %esp

    /* Save Multiboot Info (ebx) and Magic (eax) */
    push %eax
    push %ebx

    /* 
     * SETUP PAGING (Manually)
     * We need to map:
     * 1. Virt 0x00000000 -> Phys 0x00000000 (Identity, so we don't crash now)
     * 2. Virt 0xC0000000 -> Phys 0x00000000 (Kernel Space)
     */

    /* 1. Get Physical Address of Page Table 0 (PT0) */
    mov $(V2P(boot_pt0)), %edi
    
    /* 2. Map 0-4MB to this PT (fill 1024 entries) */
    /* Entry 0: Addr 0 | Present | RW */
    mov $0, %esi
    mov $1023, %ecx
1:
    cmpl $_kernel_physical_end, %esi /* Only map what we need? Nah, map 4MB */
    mov %esi, %edx
    or $3, %edx        /* Present + RW */
    mov %edx, (%edi)
    add $4096, %esi
    add $4, %edi
    loop 1b

    /* 3. Get Physical Address of Page Directory */
    mov $(V2P(boot_pd)), %edi

    /* 4. Link PT0 to PD at index 0 (Identity Map 0-4MB) */
    mov $(V2P(boot_pt0)), %edx
    or $3, %edx
    mov %edx, (%edi)

    /* 5. Link PT0 to PD at index 768 (3GB mark -> 0xC0000000) */
    /* 768 * 4MB = 3072MB = 3GB */
    mov %edx, 3072(%edi) /* Offset 768 * 4 bytes = 3072 */

    /* 6. Recursive Mapping (Optional, good for VMM later) at index 1023 */
    mov $(V2P(boot_pd)), %edx
    or $3, %edx
    mov %edx, 4092(%edi)

    /* 7. Load CR3 */
    mov $(V2P(boot_pd)), %ecx
    mov %ecx, %cr3

    /* 8. Enable Paging (Set PG bit in CR0) */
    mov %cr0, %ecx
    or $0x80000000, %ecx
    mov %ecx, %cr0

    /* 
     * PAGING IS ON!
     * We are still executing low code (thanks to identity map).
     * Now jump to the higher half!
     */
    lea higher_half_start, %ecx
    jmp *%ecx

higher_half_start:
    /* 
     * We are now running at 0xC0xxxxxx.
     * We can safely remove the identity mapping (index 0) if we want,
     * but let's leave it for C code to clean up.
     */

    /* Unmap the low 4MB (Security: Null pointer deref should fault now!) */
    movl $0, boot_pd
    invlpg 0

    /* Update Stack Pointer to be Virtual */
    /* (Currently ESP points to physical, which is valid mapped, 
        but let's fix it to use the virtual range) */
    add $KERNEL_VIRT_BASE, %esp

    /* Restore Multiboot args (popped from stack) */
    pop %ebx
    pop %eax

    /* 
     * Pass args to kernel_main.
     * NOTE: 'ebx' (multiboot info) is a PHYSICAL address.
     * kernel_main might need to map it or we identity map enough RAM.
     * Since we kept identity mapping for now (or just unmapped 0-4MB?), 
     * Wait, I unmapped 0. If multiboot struct is in low mem, we lost access.
     * Let's Re-map 0 temporarily inside C or pass physical address and let C handle it.
     * For now, let's NOT unmap 0 in asm to be safe, let C do it.
     */
    
    /* Re-map low memory for safety until PMM parses tags */
    mov $(V2P(boot_pt0)), %edx
    or $3, %edx
    mov %edx, boot_pd

    /* Call C Kernel */
    /* void kernel_main(uint32_t magic, uint32_t phys_addr) */
    push %ebx
    push %eax
    call kernel_main

    /* Hang */
    cli
1:  hlt
    jmp 1b

/* Global Paging Structures (Pre-allocated in BSS) */
.section .bss
.align 4096
.global boot_pd
boot_pd:
    .skip 4096
.global boot_pt0
boot_pt0:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384
stack_top:

/* Helper symbol for map loop limit */
_kernel_physical_end:
