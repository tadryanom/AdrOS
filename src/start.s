MULTIBOOT_PAGE_ALIGN   equ 0x1
MULTIBOOT_MEMORY_INFO  equ 0x2
MULTIBOOT_VIDEO_MODE   equ 0x4
MULTIBOOT_HEADER_MAGIC equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
MULTIBOOT_CHECKSUM     equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

VIDEO_MODE             equ 0x1    ; 0 = graphical, 1 = text
VIDEO_WIDTH            equ 0x0    ; xx caracteres or pixels of width, 0 = undefined
VIDEO_HEIGHT           equ 0x0    ; xx caracteres or pixels of width, 0 = undefined
VIDEO_DEPTH            equ 0x0    ; 0 Undefined

KERNEL_STACK_SIZE      equ 0x4000 ; Stack size (16384 bytes)

    extern kmain                  ; Import kmain and puts symbols
    extern puts
    global kernel_entry           ; Export kernel_entry symbol
    global gdt_flush              ; Export gdt_flush, tss_flush and idt_flush symbols
    global tss_flush
    global idt_flush

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

    push 0x0
    popf                          ; Reset EFLAGS register

    push esp                      ; Push the pointer of the initial stack
    push ebx                      ; Push the pointer to the Multiboot information structure.
    push eax                      ; Push the magic value.
    call kmain                    ; I will call the kmain() function to do other things

    push haltmsg
    call puts                     ; Print on screen "System has halted" message

    cli                           ; Disables all hardware interrupts
    hlt                           ; Do halt CPU
    jmp $                         ; Kernel goes into infinite loop

kernel_entry_size: dd $-$$

gdt_flush:            ; Allows the C code to call gdt_flush().
    mov eax, [esp+4]  ; Get the pointer to the GDT, passed as a parameter.
    lgdt [eax]        ; Load the new GDT pointer

    mov ax, 0x10      ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax        ; Load all data segment selectors
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush   ; 0x08 is the offset to our code segment: Far jump!
.flush:
    ret

tss_flush:            ; Allows our C code to call tss_flush().
    mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                      ; 0x28, as it is the 5th selector and each is 8 bytes
                      ; long, but we set the bottom two bits (making 0x2B)
                      ; so that it has an RPL of 3, not zero.
    ltr ax            ; Load 0x2B into the task state register.
    ret

idt_flush:            ; Allows the C code to call idt_flush().
    mov eax, [esp+4]  ; Get the pointer to the IDT, passed as a parameter. 
    lidt [eax]        ; Load the IDT pointer.
    ret

    section .data                 ; Beginning of section with initialized data
haltmsg: db 'System has halted.',0x0

    section .bss                  ; Beginning of uninitialized data section
    align 0x10                    ; Aligns to 16 bytes
kernel_stack_bottom:
    resb KERNEL_STACK_SIZE
kernel_stack_top:
    resb 0x1
