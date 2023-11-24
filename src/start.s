MULTIBOOT_PAGE_ALIGN   equ 0x1
MULTIBOOT_MEMORY_INFO  equ 0x2
MULTIBOOT_VIDEO_MODE   equ 0x4
MULTIBOOT_HEADER_MAGIC equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
MULTIBOOT_CHECKSUM     equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

VIDEO_MODE             equ 0x1    ; 0 = graphical, 1 = text
VIDEO_WIDTH            equ 0x50   ; 80 caracteres of width
VIDEO_HEIGHT           equ 0x19   ; 25 caracteres of height
VIDEO_DEPTH            equ 0x0    ;  0 Undefined

KERNEL_STACK_SIZE      equ 0x4000 ; Stack size (16384 bytes)

    global kernel_entry           ; Export kernel_entry symbol
    extern kmain                  ; Import kmain and printf symbols
    extern printf

    section .multiboot
    align 0x4                     ; Aligns to 4 bytes
multiboot_header:
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM
    dd 0x0,0x0,0x0,0x0,0x0
    dd VIDEO_MODE
    dd VIDEO_WIDTH
    dd VIDEO_HEIGHT
    dd VIDEO_DEPTH

    section .text                 ; Start of code section
kernel_entry:                     ; Entry point for the kernel defined in the linker script
    mov esp, kernel_stack_top     ; Initialize the stack pointer.

    push 0x0                      ; Reset EFLAGS
    popf

    push ebx                      ; Push the pointer to the Multiboot information structure.
    push eax                      ; Push the magic value.
    call kmain                    ; I will call the kmain() function for the kernel to do other things

    push haltmsg                  ; Print on screen "System has halted" message
    call  printf

    cli                           ; Disables all hardware interrupts
    hlt
    jmp $                         ; Kernel goes into infinite loop

    section .data                 ; Beginning of section with initialized data
haltmsg: db 'System has halted.',0x0

    section .bss                  ; Beginning of uninitialized data section
    align 0x10                    ; Aligns to 16 bytes
kernel_stack_bottom:
    resb KERNEL_STACK_SIZE
kernel_stack_top:
