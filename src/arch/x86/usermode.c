#include <stdint.h>
#include <stddef.h>

#include "pmm.h"
#include "vmm.h"
#include "uart_console.h"
#include "utils.h"

#if defined(__i386__)

static void* pmm_alloc_page_low_16mb(void) {
    for (int tries = 0; tries < 4096; tries++) {
        void* p = pmm_alloc_page();
        if (!p) return NULL;
        if ((uintptr_t)p < 0x01000000U) {
            return p;
        }
        pmm_free_page(p);
    }
    return NULL;
}

__attribute__((noreturn)) static void enter_usermode(uintptr_t user_eip, uintptr_t user_esp) {
    __asm__ volatile(
        "cli\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushl $0x23\n"         /* ss */
        "pushl %[esp]\n"        /* esp */
        "pushfl\n"
        "popl %%eax\n"
        "orl $0x200, %%eax\n"   /* IF=1 */
        "pushl %%eax\n"
        "pushl $0x1B\n"         /* cs */
        "pushl %[eip]\n"        /* eip */
        "iret\n"
        :
        : [eip] "r"(user_eip), [esp] "r"(user_esp)
        : "eax", "memory"
    );

    __builtin_unreachable();
}

void x86_usermode_test_start(void) {
    uart_print("[USER] Starting ring3 test...\n");

    const uintptr_t user_code_vaddr = 0x00400000U;
    const uintptr_t user_stack_vaddr = 0x00800000U;

    void* code_phys = pmm_alloc_page_low_16mb();
    void* stack_phys = pmm_alloc_page_low_16mb();
    if (!code_phys || !stack_phys) {
        uart_print("[USER] OOM allocating user pages.\n");
        return;
    }

    vmm_map_page((uint64_t)(uintptr_t)code_phys, (uint64_t)user_code_vaddr,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
    vmm_map_page((uint64_t)(uintptr_t)stack_phys, (uint64_t)user_stack_vaddr,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);

    static const uint8_t user_prog[] = {
        0xB8, 0x01, 0x00, 0x00, 0x00,             /* mov eax, 1 (SYSCALL_WRITE) */
        0xBB, 0x01, 0x00, 0x00, 0x00,             /* mov ebx, 1 (stdout) */
        0xB9, 0x40, 0x00, 0x40, 0x00,             /* mov ecx, 0x00400040 */
        0xBA, 0x12, 0x00, 0x00, 0x00,             /* mov edx, 0x12 */
        0xCD, 0x80,                               /* int 0x80 */
        0xB8, 0x02, 0x00, 0x00, 0x00,             /* mov eax, 2 (SYSCALL_EXIT) */
        0xCD, 0x80,                               /* int 0x80 */
        0xEB, 0xFE                                /* jmp $ (loop) */
    };

    static const char msg[] = "Hello from ring3!\n";

    memcpy((void*)(uintptr_t)code_phys, user_prog, sizeof(user_prog));
    memcpy((void*)((uintptr_t)code_phys + 0x40), msg, sizeof(msg) - 1);

    uintptr_t user_esp = user_stack_vaddr + 4096;
    enter_usermode(user_code_vaddr, user_esp);
}

#endif
