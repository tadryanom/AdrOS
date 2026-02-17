#include <stdint.h>
#include <stddef.h>

#include "pmm.h"
#include "vmm.h"
#include "console.h"
#include "process.h"
#include "utils.h"
#include "arch/x86/usermode.h"
#include "arch/x86/idt.h"

#if defined(__i386__)

enum {
    SYSCALL_WRITE_NO = 1,
    SYSCALL_EXIT_NO  = 2,
};

struct emitter {
    uint8_t* buf;
    size_t pos;
};

struct patch {
    size_t at;
    size_t target;
};

static void emit8(struct emitter* e, uint8_t v) { e->buf[e->pos++] = v; }
static void emit32(struct emitter* e, uint32_t v) {
    e->buf[e->pos++] = (uint8_t)(v & 0xFF);
    e->buf[e->pos++] = (uint8_t)((v >> 8) & 0xFF);
    e->buf[e->pos++] = (uint8_t)((v >> 16) & 0xFF);
    e->buf[e->pos++] = (uint8_t)((v >> 24) & 0xFF);
}

static void emit_mov_eax_imm(struct emitter* e, uint32_t imm) { emit8(e, 0xB8); emit32(e, imm); }
static void emit_mov_ebx_imm(struct emitter* e, uint32_t imm) { emit8(e, 0xBB); emit32(e, imm); }
static void emit_mov_ecx_imm(struct emitter* e, uint32_t imm) { emit8(e, 0xB9); emit32(e, imm); }
static void emit_mov_edx_imm(struct emitter* e, uint32_t imm) { emit8(e, 0xBA); emit32(e, imm); }
static void emit_int80(struct emitter* e) { emit8(e, 0xCD); emit8(e, 0x80); }
static void emit_cmp_eax_imm(struct emitter* e, uint32_t imm) { emit8(e, 0x3D); emit32(e, imm); }

static void emit_jne_rel8_patch(struct emitter* e, struct patch* p, size_t target) {
    emit8(e, 0x75);
    p->at = e->pos;
    p->target = target;
    emit8(e, 0x00);
}

static void emit_jmp_rel8_patch(struct emitter* e, struct patch* p, size_t target) {
    emit8(e, 0xEB);
    p->at = e->pos;
    p->target = target;
    emit8(e, 0x00);
}

static void patch_rel8(uint8_t* buf, size_t at, size_t target) {
    int32_t rel = (int32_t)target - (int32_t)(at + 1);
    buf[at] = (uint8_t)(int8_t)rel;
}

/* User pages can be anywhere in physical memory on 32-bit PAE. */

__attribute__((noreturn)) void x86_enter_usermode(uintptr_t user_eip, uintptr_t user_esp) {
    kprintf("[USER] enter ring3 eip=0x%x esp=0x%x\n",
            (unsigned)user_eip, (unsigned)user_esp);

    __asm__ volatile(
        "cli\n"
        "mov $0x23, %%ax\n"     /* user data segment (GDT entry 4, RPL=3) */
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushl $0x23\n"         /* ss */
        "pushl %[esp]\n"        /* esp */
        "pushl $0x202\n"        /* eflags: IF=1 */
        "pushl $0x1B\n"         /* cs */
        "pushl %[eip]\n"        /* eip */
        "iret\n"
        :
        : [eip] "r"(user_eip), [esp] "r"(user_esp)
        : "memory", "eax"
    );

    __builtin_unreachable();
}

__attribute__((noreturn)) void x86_enter_usermode_regs(const struct registers* regs) {
    if (!regs) {
        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    // Layout follows include/arch/x86/idt.h struct registers.
    // struct registers { gs(0), ds(4), edi(8), esi(12), ebp(16),
    //   esp(20), ebx(24), edx(28), ecx(32), eax(36),
    //   int_no(40), err_code(44), eip(48), cs(52), eflags(56),
    //   useresp(60), ss(64) };
    const uint32_t eflags = (regs->eflags | 0x200U);

    /* Use ESI as scratch to hold regs pointer, since we'll overwrite
     * EBP manually inside the asm block. ESI is restored from the
     * struct before iret. */
    __asm__ volatile(
        "cli\n"
        "mov %[r], %%esi\n"

        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        "pushl $0x23\n"           /* ss */
        "pushl 60(%%esi)\n"       /* useresp */
        "pushl %[efl]\n"          /* eflags */
        "pushl $0x1B\n"           /* cs */
        "pushl 48(%%esi)\n"       /* eip */

        "mov  8(%%esi), %%edi\n"  /* edi */
        "mov 16(%%esi), %%ebp\n"  /* ebp */
        "mov 24(%%esi), %%ebx\n"  /* ebx */
        "mov 28(%%esi), %%edx\n"  /* edx */
        "mov 32(%%esi), %%ecx\n"  /* ecx */
        "mov 36(%%esi), %%eax\n"  /* eax */
        "mov 12(%%esi), %%esi\n"  /* esi (last — self-overwrite) */
        "iret\n"
        :
        : [r] "r"(regs),
          [efl] "r"(eflags)
        : "memory", "cc"
    );

    __builtin_unreachable();
}

void x86_usermode_test_start(void) {
    kprintf("[USER] Starting ring3 test...\n");

    const uintptr_t user_code_vaddr = 0x00400000U;
    const uintptr_t user_stack_vaddr = 0x40000000U;

    void* code_phys = pmm_alloc_page();
    void* stack_phys = pmm_alloc_page();
    if (!code_phys || !stack_phys) {
        kprintf("[USER] OOM allocating user pages.\n");
        return;
    }

    const uintptr_t base = user_code_vaddr;
    const uint32_t addr_t1_ok = (uint32_t)(base + 0x200);
    const uint32_t addr_t1_fail = (uint32_t)(base + 0x210);
    const uint32_t addr_t2_ok = (uint32_t)(base + 0x220);
    const uint32_t addr_t2_fail = (uint32_t)(base + 0x230);
    const uint32_t addr_t3_ok = (uint32_t)(base + 0x240);
    const uint32_t addr_t3_fail = (uint32_t)(base + 0x250);
    const uint32_t addr_msg = (uint32_t)(base + 0x300);

    const uint32_t t1_ok_len = 6;
    const uint32_t t1_fail_len = 8;
    const uint32_t t2_ok_len = 6;
    const uint32_t t2_fail_len = 8;
    const uint32_t t3_ok_len = 6;
    const uint32_t t3_fail_len = 8;
    const uint32_t msg_len = 18;

    /* Access the physical page via the kernel higher-half mapping (P2V)
     * instead of relying on an identity mapping that may not exist. */
    const uintptr_t code_kva = (uintptr_t)code_phys + 0xC0000000U;
    struct emitter e = { .buf = (uint8_t*)code_kva, .pos = 0 };

    /* T1: write(valid buf) -> t1_ok_len */
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_t1_ok);
    emit_mov_edx_imm(&e, t1_ok_len);
    emit_int80(&e);
    emit_cmp_eax_imm(&e, t1_ok_len);
    struct patch t1_fail_jne = {0};
    emit_jne_rel8_patch(&e, &t1_fail_jne, 0);
    struct patch t1_to_t2 = {0};
    emit_jmp_rel8_patch(&e, &t1_to_t2, 0);
    /* FAIL label */
    size_t t1_fail_pos = e.pos;
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_t1_fail);
    emit_mov_edx_imm(&e, t1_fail_len);
    emit_int80(&e);
    size_t t2_pos = e.pos;

    /* T2: write(valid buf) -> t2_ok_len */
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_t2_ok);
    emit_mov_edx_imm(&e, t2_ok_len);
    emit_int80(&e);
    emit_cmp_eax_imm(&e, t2_ok_len);
    struct patch t2_fail_jne = {0};
    emit_jne_rel8_patch(&e, &t2_fail_jne, 0);
    struct patch t2_to_t3 = {0};
    emit_jmp_rel8_patch(&e, &t2_to_t3, 0);
    /* FAIL label */
    size_t t2_fail_pos = e.pos;
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_t2_fail);
    emit_mov_edx_imm(&e, t2_fail_len);
    emit_int80(&e);
    size_t t3_pos = e.pos;

    /* T3: write(valid buf) -> msg_len */
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_msg);
    emit_mov_edx_imm(&e, msg_len);
    emit_int80(&e);
    emit_cmp_eax_imm(&e, msg_len);
    struct patch t3_fail_jne = {0};
    emit_jne_rel8_patch(&e, &t3_fail_jne, 0);
    /* OK print */
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_t3_ok);
    emit_mov_edx_imm(&e, t3_ok_len);
    emit_int80(&e);
    struct patch t3_to_exit = {0};
    emit_jmp_rel8_patch(&e, &t3_to_exit, 0);
    /* FAIL label */
    size_t t3_fail_pos = e.pos;
    emit_mov_eax_imm(&e, SYSCALL_WRITE_NO);
    emit_mov_ebx_imm(&e, 1);
    emit_mov_ecx_imm(&e, addr_t3_fail);
    emit_mov_edx_imm(&e, t3_fail_len);
    emit_int80(&e);
    size_t exit_pos = e.pos;
    emit_mov_eax_imm(&e, SYSCALL_EXIT_NO);
    emit_mov_ebx_imm(&e, 0);
    emit_int80(&e);
    emit8(&e, 0xEB);
    emit8(&e, 0xFE);

    patch_rel8(e.buf, t1_fail_jne.at, t1_fail_pos);
    patch_rel8(e.buf, t1_to_t2.at, t2_pos);
    patch_rel8(e.buf, t2_fail_jne.at, t2_fail_pos);
    patch_rel8(e.buf, t2_to_t3.at, t3_pos);
    patch_rel8(e.buf, t3_fail_jne.at, t3_fail_pos);
    patch_rel8(e.buf, t3_to_exit.at, exit_pos);

    memcpy((void*)(code_kva + 0x200), "T1 OK\n", t1_ok_len);
    memcpy((void*)(code_kva + 0x210), "T1 FAIL\n", t1_fail_len);
    memcpy((void*)(code_kva + 0x220), "T2 OK\n", t2_ok_len);
    memcpy((void*)(code_kva + 0x230), "T2 FAIL\n", t2_fail_len);
    memcpy((void*)(code_kva + 0x240), "T3 OK\n", t3_ok_len);
    memcpy((void*)(code_kva + 0x250), "T3 FAIL\n", t3_fail_len);
    memcpy((void*)(code_kva + 0x300), "Hello from ring3!\n", msg_len);

    /* Create a private address space so the ring3 user pages do NOT
     * pollute kernel_as (which is shared by all kernel threads).
     * Code/data was emitted above via P2V (kernel higher-half mapping);
     * now we switch to the new AS and map the physical pages at their
     * user virtual addresses. */
    uintptr_t ring3_as = vmm_as_create_kernel_clone();
    if (!ring3_as) {
        kprintf("[USER] Failed to create ring3 address space.\n");
        pmm_free_page(code_phys);
        pmm_free_page(stack_phys);
        return;
    }

    current_process->addr_space = ring3_as;
    vmm_as_activate(ring3_as);

    vmm_map_page((uint64_t)(uintptr_t)code_phys, (uint64_t)user_code_vaddr,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
    vmm_map_page((uint64_t)(uintptr_t)stack_phys, (uint64_t)user_stack_vaddr,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);

    uintptr_t user_esp = user_stack_vaddr + 4096;
    x86_enter_usermode(user_code_vaddr, user_esp);
}

#endif
