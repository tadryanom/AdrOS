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

    /* End tag */
    .align 8
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
     * SETUP PAE PAGING
     * PAE uses 3-level page tables:
     *   PDPT (4 entries x 8 bytes = 32 bytes, 32-byte aligned)
     *   PD[0..3] (512 entries x 8 bytes = 4KB each)
     *   PT (512 entries x 8 bytes = 4KB each)
     *
     * Address split: bits 31:30 = PDPT, bits 29:21 = PD, bits 20:12 = PT
     *
     * We map 0-16MB identity + higher-half using 8 page tables (2MB each).
     * PD[0] entries 0..7 = identity map (PT boot_pt0..boot_pt7)
     * PD[3] entries 0..7 = higher-half map (same PTs)
     * Recursive: PD[3][508..511] point to PD[0..3]
     */

    /* --- Zero out all paging structures --- */
    mov $boot_pdpt, %edi
    sub $KERNEL_VIRT_BASE, %edi
    xor %eax, %eax
    mov $(32 + 4*4096 + 8*4096), %ecx  /* PDPT + 4 PDs + 8 PTs in bytes */
    shr $2, %ecx                        /* /4 for stosl */
    rep stosl

    /* --- Fill page tables (boot_pt0..boot_pt7) with 64-bit PAE entries --- */
    /* Each PT has 512 entries x 8 bytes. 8 PTs cover 8*512*4KB = 16MB. */
    mov $boot_pt0, %edi
    sub $KERNEL_VIRT_BASE, %edi
    xor %esi, %esi              /* physical address = 0 */
    mov $(512*8), %ecx          /* 8 PTs x 512 entries = 4096 entries */
1:
    mov %esi, %eax
    or $0x03, %eax              /* PRESENT | RW */
    mov %eax, (%edi)            /* low 32 bits */
    movl $0, 4(%edi)            /* high 32 bits = 0 */
    add $4096, %esi
    add $8, %edi
    dec %ecx
    jnz 1b

    /* --- Set up PD[0]: identity map entries 0..7 -> boot_pt0..boot_pt7 --- */
    mov $boot_pd0, %edi
    sub $KERNEL_VIRT_BASE, %edi
    mov $boot_pt0, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov $8, %ecx
2:
    mov %edx, %eax
    or $0x03, %eax              /* PRESENT | RW */
    mov %eax, (%edi)
    movl $0, 4(%edi)
    add $4096, %edx
    add $8, %edi
    dec %ecx
    jnz 2b

    /* --- Set up PD[3]: higher-half entries 0..7 -> boot_pt0..boot_pt7 --- */
    mov $boot_pd3, %edi
    sub $KERNEL_VIRT_BASE, %edi
    mov $boot_pt0, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov $8, %ecx
3:
    mov %edx, %eax
    or $0x03, %eax              /* PRESENT | RW */
    mov %eax, (%edi)
    movl $0, 4(%edi)
    add $4096, %edx
    add $8, %edi
    dec %ecx
    jnz 3b

    /* --- Recursive mapping: PD[3][508..511] -> PD[0..3] --- */
    mov $boot_pd3, %edi
    sub $KERNEL_VIRT_BASE, %edi
    /* entry 508 = offset 508*8 = 4064 */
    add $4064, %edi

    mov $boot_pd0, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x03, %eax
    mov %eax, (%edi)            /* PD[3][508] -> PD[0] */
    movl $0, 4(%edi)

    mov $boot_pd1, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x03, %eax
    mov %eax, 8(%edi)           /* PD[3][509] -> PD[1] */
    movl $0, 12(%edi)

    mov $boot_pd2, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x03, %eax
    mov %eax, 16(%edi)          /* PD[3][510] -> PD[2] */
    movl $0, 20(%edi)

    mov $boot_pd3, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x03, %eax
    mov %eax, 24(%edi)          /* PD[3][511] -> PD[3] */
    movl $0, 28(%edi)

    /* --- Set up PDPT: 4 entries -> PD[0..3] --- */
    mov $boot_pdpt, %edi
    sub $KERNEL_VIRT_BASE, %edi

    mov $boot_pd0, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x01, %eax              /* PRESENT only (PDPT entries have limited flags) */
    mov %eax, (%edi)
    movl $0, 4(%edi)

    mov $boot_pd1, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x01, %eax
    mov %eax, 8(%edi)
    movl $0, 12(%edi)

    mov $boot_pd2, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x01, %eax
    mov %eax, 16(%edi)
    movl $0, 20(%edi)

    mov $boot_pd3, %edx
    sub $KERNEL_VIRT_BASE, %edx
    mov %edx, %eax
    or $0x01, %eax
    mov %eax, 24(%edi)
    movl $0, 28(%edi)

    /* --- Enable PAE in CR4 --- */
    mov %cr4, %ecx
    or $0x20, %ecx              /* CR4.PAE = bit 5 */
    mov %ecx, %cr4

    /* --- Load CR3 with PDPT physical address --- */
    mov $boot_pdpt, %ecx
    sub $KERNEL_VIRT_BASE, %ecx
    mov %ecx, %cr3

    /* --- Enable Paging (Set PG bit in CR0) --- */
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

    /* Build arch_boot_args (in .bss) and call arch_early_setup(args) */
    mov $arch_boot_args, %ecx
    mov %eax, 0(%ecx)          /* args->a0 = multiboot magic */
    mov %ebx, 4(%ecx)          /* args->a1 = multiboot info phys */
    movl $0, 8(%ecx)           /* args->a2 = 0 */
    movl $0, 12(%ecx)          /* args->a3 = 0 */

    push %ecx
    call arch_early_setup
    add $4, %esp

    /* Hang */
    cli
1:  hlt
    jmp 1b

/* Global PAE Paging Structures (Pre-allocated in BSS) */
.section .bss
.align 32
.global boot_pdpt
boot_pdpt:
    .skip 32

.align 4096
.global boot_pd0
boot_pd0:
    .skip 4096
.global boot_pd1
boot_pd1:
    .skip 4096
.global boot_pd2
boot_pd2:
    .skip 4096
.global boot_pd3
boot_pd3:
    .skip 4096

.global boot_pt0
boot_pt0:
    .skip 4096*8

.align 16
.global arch_boot_args
arch_boot_args:
    .skip 16

.align 16
stack_bottom:
    .skip 16384
.global stack_top
stack_top:

.section .note.GNU-stack,"",@progbits

/* Helper symbol for map loop limit */
_kernel_physical_end:
