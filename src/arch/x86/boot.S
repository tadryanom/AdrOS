/*
 * AdrOS - x86 Higher Half Kernel Bootstrap
 */

/* Constants */
.set MB_MAGIC, 0xE85250D6
.set MB_ARCH,  0          /* i386 */
.set KERNEL_VIRT_BASE, 0xC0000000

/* 
 * Convert virtual symbols to physical addresses before paging is enabled
 * by subtracting KERNEL_VIRT_BASE.
 */

/* .multiboot_header section (Goes to the .boot file in the linker) */
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

/** .boot_text section (Goes to the .boot file in the linker)
 * This runs in 1MB (Identity Mapped by the Linker)
 */
.section .boot_text
.global _start
.type _start, @function

_start:
    /* Disable interrupts until the kernel installs a valid IDT */
    cli
    /* 
     * We are loaded at 1MB physical. Paging is OFF.
     * CPU is executing instructions here.
     * ESP is unknown.
     */

    /* Setup a temporary stack (using physical address) */
    mov $stack_top, %esp
    sub $KERNEL_VIRT_BASE, %esp

    /* Save Multiboot Info (ebx) and Magic (eax) */
    push %eax
    push %ebx

    /* Mask PIC (disable all IRQs) until kernel remaps/unmasks */
    movb $0xFF, %al
    outb %al, $0x21
    outb %al, $0xA1

    /* 
     * SETUP PAGING (Manually)
     * We need to map:
     * 1. Virt 0x00000000 -> Phys 0x00000000 (Identity, so we don't crash now)
     * 2. Virt 0xC0000000 -> Phys 0x00000000 (Kernel Space)
     */

    /*
     * Map 0-16MB using 4 page tables.
     * With Multiboot2 info copied in arch_start and initrd mapped via VMM,
     * we only need a small identity window for early bring-up.
     */

    /* Fill PTs (boot_pt0..boot_pt3) */
    mov $boot_pt0, %edi
    sub $KERNEL_VIRT_BASE, %edi /* Physical address of PT0 */
    xor %ebx, %ebx              /* pt_index = 0 */

1:
    /* Fill current PT with 0x003 | (pt_index*4MB + i*4KB) */
    mov %ebx, %eax
    shl $22, %eax               /* base = pt_index * 4MB */
    mov %eax, %esi

    mov $1024, %ecx
2:
    mov %esi, %edx
    or $3, %edx
    mov %edx, (%edi)
    add $4096, %esi
    add $4, %edi
    loop 2b

    inc %ebx
    cmp $4, %ebx
    jne 1b

    /* 3. Get Physical Address of Page Directory */
    mov $boot_pd, %edi
    sub $KERNEL_VIRT_BASE, %edi

    /* Link PT0..PT3 into PD for both identity and higher-half mapping */
    mov $boot_pt0, %edx
    sub $KERNEL_VIRT_BASE, %edx /* pt_phys = physical address of PT0 */
    mov $0, %ebx                /* i = 0 */

3:
    mov %edx, %eax
    or $3, %eax
    mov %eax, (%edi,%ebx,4)         /* PD[i] */
    mov %eax, 3072(%edi,%ebx,4)     /* PD[768+i] */
    add $4096, %edx
    inc %ebx
    cmp $4, %ebx
    jne 3b

    /* 6. Recursive Mapping (Optional, good for VMM later) at index 1023 */
    mov $boot_pd, %edx
    sub $KERNEL_VIRT_BASE, %edx
    or $3, %edx
    mov %edx, 4092(%edi)

    /* 7. Load CR3 */
    mov $boot_pd, %ecx
    sub $KERNEL_VIRT_BASE, %ecx
    mov %ecx, %cr3

    /* 8. Enable Paging (Set PG bit in CR0) */
    mov %cr0, %ecx
    or $0x80000000, %ecx /* Bit 31 (PG) e Bit 0 (PE - Protection Enable) */
    mov %ecx, %cr0

    /* 
     * PAGING IS ON!
     * We are still executing low code (thanks to identity map).
     * Now jump to the higher half!
     */
    lea higher_half_start, %ecx
    jmp *%ecx

/** From here, we switch to the default .text section.
 * The linker will place this in the 3GB area.
 */
.section .text 
higher_half_start:
    /* 
     * We are now running at 0xC0xxxxxx.
     * We can safely remove the identity mapping (index 0) if we want,
     * but let's leave it for C code to clean up.
     */

    /* Update Stack Pointer to be Virtual */
    /* (Currently ESP points to physical, which is valid mapped, 
     *  but let's fix it to use the virtual range) */
    add $KERNEL_VIRT_BASE, %esp

    /* Restore Multiboot args (popped from stack) */
    pop %ebx /* EBX now has the Multiboot Info address */
    pop %eax /* EAX now has the Magic Number */

    /* Build arch_boot_args (in .bss) and call arch_start(args) */
    mov $arch_boot_args, %ecx
    mov %eax, 0(%ecx)          /* args->a0 = multiboot magic */
    mov %ebx, 4(%ecx)          /* args->a1 = multiboot info phys */
    movl $0, 8(%ecx)           /* args->a2 = 0 */
    movl $0, 12(%ecx)          /* args->a3 = 0 */

    push %ecx
    call arch_start
    add $4, %esp

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
    .skip 4096*4

.align 16
.global arch_boot_args
arch_boot_args:
    .skip 16

.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .note.GNU-stack,"",@progbits

/* Helper symbol for map loop limit */
_kernel_physical_end:
