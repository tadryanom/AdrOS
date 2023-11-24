MULTIBOOT_PAGE_ALIGN   equ 0x1
MULTIBOOT_MEMORY_INFO  equ 0x2
MULTIBOOT_VIDEO_MODE   equ 0x4
MULTIBOOT_HEADER_MAGIC equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
MULTIBOOT_CHECKSUM     equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

VIDEO_MODE             equ 0x1  ; 0 - graphical, 1 - text
VIDEO_WIDTH            equ 0x50 ; 80
VIDEO_HEIGHT           equ 0x19 ; 25
VIDEO_DEPTH            equ 0x0  ;  0

KERNEL_STACK_SIZE      equ 0x4000 ; Stack size in bytes

section .text                     ; Start of code section
global start                      ; Global declaration for kernel ELF entry point
start:                            ; Entry point for the kernel defined in the linker script
    jmp kernel_entry

align 0x4                         ; Aligns to 4 bytes
multiboot_header:
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM
    dd 0x0,0x0,0x0,0x0,0x0
    dd VIDEO_MODE
    dd VIDEO_WIDTH
    dd VIDEO_HEIGHT
    dd VIDEO_DEPTH

kernel_entry:
    mov esp, kernel_stack + KERNEL_STACK_SIZE ; Initialize the stack pointer.

    push 0x0                      ; Reset EFLAGS
    popf

    push ebx                      ; Push the pointer to the Multiboot information structure.
    push eax                      ; Push the magic value.
    extern kmain
    call kmain                    ; I will call the kmain() function for the kernel to do other things

    push haltmsg
    extern printf
    call  printf

.loop:
    cli                           ; Disables all hardware interrupts
    hlt
    jmp .loop

section .data                     ; Beginning of section with initialized data
align 0x4                         ; Aligns to 4 bytes
haltmsg: db 'System has halted.',0x0

section .bss                      ; Beginning of uninitialized data section
align 0x4                         ; Aligns to 4 bytes
kernel_stack:
    resb KERNEL_STACK_SIZE
