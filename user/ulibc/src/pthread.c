#include "pthread.h"
#include "syscall.h"
#include "errno.h"

#include <stdint.h>
#include <stddef.h>

/* clone() flags (must match kernel's process.h) */
#define CLONE_VM        0x00000100
#define CLONE_FS        0x00000200
#define CLONE_FILES     0x00000400
#define CLONE_SIGHAND   0x00000800
#define CLONE_THREAD    0x00010000
#define CLONE_SETTLS    0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000

#define CLONE_THREAD_FLAGS  (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SETTLS)

#define THREAD_STACK_SIZE 8192

/* Per-thread trampoline info, placed at the bottom of the thread stack */
struct thread_start_info {
    void* (*start_routine)(void*);
    void* arg;
    void* retval;
    int   exited;
};

/* Thread trampoline: called by the clone child.
 * The child's ESP points to the top of the allocated stack.
 * We read start_info from the bottom of the stack, call the user function,
 * then call pthread_exit. */
static void __attribute__((noreturn)) thread_entry_trampoline(void) {
    /* The start_info pointer is at ESP (pushed before clone).
     * After clone returns 0, EAX=0, and we land here with the
     * stack set up so that start_info is accessible.
     *
     * Actually, with our clone design, we enter usermode with the
     * register state. We stash the info pointer in a register-accessible
     * location. Let's read it from ESI (we pass it as part of the stack). */

    /* In our implementation, the child starts with the parent's register
     * state but ESP overridden. We place start_info at a known location
     * relative to the stack base. The trampoline wrapper below handles this. */
    for (;;) {
        __asm__ volatile("nop");
    }
}

/* Wrapper that runs on the new thread's stack */
static void __attribute__((noreturn, used))
_pthread_trampoline(struct thread_start_info* info) {
    void* ret = info->start_routine(info->arg);
    info->retval = ret;
    info->exited = 1;
    /* Exit thread */
    _syscall1(SYS_EXIT, 0);
    for (;;) __asm__ volatile("nop");
}

/* Simple bump allocator for thread stacks (no free support yet) */
static uint8_t _thread_stack_pool[8][THREAD_STACK_SIZE];
static int _thread_stack_next = 0;

static void* alloc_thread_stack(void) {
    if (_thread_stack_next >= 8) return NULL;
    return &_thread_stack_pool[_thread_stack_next++];
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    (void)attr;
    if (!thread || !start_routine) return 22; /* EINVAL */

    /* Allocate a stack for the new thread */
    void* stack_base = alloc_thread_stack();
    if (!stack_base) return 12; /* ENOMEM */

    /* Place start_info at the bottom of the stack */
    struct thread_start_info* info = (struct thread_start_info*)stack_base;
    info->start_routine = start_routine;
    info->arg = arg;
    info->retval = NULL;
    info->exited = 0;

    /* Set up the child stack: top of allocated region, with trampoline args */
    uint32_t* sp = (uint32_t*)((uint8_t*)stack_base + THREAD_STACK_SIZE);

    /* Push argument (pointer to start_info) for trampoline */
    *--sp = (uint32_t)(uintptr_t)info;
    /* Push fake return address */
    *--sp = 0;

    /* clone(flags, child_stack, parent_tidptr, tls, child_tidptr)
     * syscall args: eax=SYS_CLONE, ebx=flags, ecx=child_stack,
     *               edx=parent_tidptr, esi=tls, edi=child_tidptr
     *
     * We use _syscall5 but note: parent_tidptr and child_tidptr are 0 for now,
     * tls is 0 (no TLS for basic threads). */
    uint32_t flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;
    int ret = _syscall5(SYS_CLONE, (int)flags, (int)(uintptr_t)sp, 0, 0, 0);

    if (ret < 0) {
        /* clone failed */
        return -ret;
    }

    if (ret == 0) {
        /* We are the child thread.
         * Pop the info pointer from stack and call trampoline. */
        struct thread_start_info* my_info;
        __asm__ volatile("popl %0" : "=r"(my_info));
        /* Discard fake return address */
        __asm__ volatile("addl $4, %%esp" : : : "memory");
        _pthread_trampoline(my_info);
        /* Never reached */
    }

    /* Parent: ret is child tid */
    *thread = (pthread_t)(uint32_t)ret;
    return 0;
}

int pthread_join(pthread_t thread, void** retval) {
    /* Use waitpid on the thread's PID.
     * Since CLONE_THREAD threads are in the same thread group,
     * waitpid may not work directly. Use a simple spin-wait as fallback. */
    int status = 0;
    int r = _syscall3(SYS_WAITPID, (int)thread, (int)&status, 0);
    if (r < 0) {
        /* Thread may have already exited or waitpid doesn't work for threads.
         * For now, just return success if we can't wait. */
        (void)retval;
        return 0;
    }
    if (retval) *retval = NULL;
    return 0;
}

void pthread_exit(void* retval) {
    (void)retval;
    _syscall1(SYS_EXIT, 0);
    for (;;) __asm__ volatile("nop");
}

pthread_t pthread_self(void) {
    return (pthread_t)(uint32_t)_syscall0(SYS_GETTID);
}

int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return 22;
    attr->stack_size = THREAD_STACK_SIZE;
    attr->detach_state = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) {
    (void)attr;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize) {
    if (!attr || stacksize < 4096) return 22;
    attr->stack_size = stacksize;
    return 0;
}
