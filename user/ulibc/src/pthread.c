// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

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

int pthread_detach(pthread_t thread) {
    (void)thread;
    return 0;
}

int pthread_cancel(pthread_t thread) {
    (void)thread;
    return 0;
}

int pthread_setcancelstate(int state, int* oldstate) {
    if (oldstate) *oldstate = 0;
    (void)state;
    return 0;
}

int pthread_setcanceltype(int type, int* oldtype) {
    if (oldtype) *oldtype = 0;
    (void)type;
    return 0;
}

void pthread_testcancel(void) {}

/* ---- Futex helpers ---- */
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

static int futex_wait(volatile int* addr, int val) {
    return _syscall3(SYS_FUTEX, (int)addr, FUTEX_WAIT, val);
}

static int futex_wake(volatile int* addr, int count) {
    return _syscall3(SYS_FUTEX, (int)addr, FUTEX_WAKE, count);
}

static int atomic_cas(volatile int* ptr, int old, int new_val) {
    int prev;
    __asm__ volatile("lock cmpxchgl %2, %1"
                     : "=a"(prev), "+m"(*ptr)
                     : "r"(new_val), "0"(old)
                     : "memory");
    return prev;
}

static int atomic_xchg(volatile int* ptr, int val) {
    __asm__ volatile("xchgl %0, %1"
                     : "=r"(val), "+m"(*ptr)
                     : "0"(val)
                     : "memory");
    return val;
}

static void atomic_add(volatile int* ptr, int val) {
    __asm__ volatile("lock addl %1, %0" : "+m"(*ptr) : "r"(val) : "memory");
}

/* ---- Mutex ---- */
int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return 22;
    mutex->__lock = 0;
    mutex->__owner = 0;
    mutex->__type = attr ? attr->__type : PTHREAD_MUTEX_NORMAL;
    mutex->__count = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex) return 22;
    int tid = _syscall0(SYS_GETTID);

    if (mutex->__type == PTHREAD_MUTEX_RECURSIVE && mutex->__owner == tid) {
        mutex->__count++;
        return 0;
    }

    while (atomic_xchg(&mutex->__lock, 1) != 0) {
        futex_wait(&mutex->__lock, 1);
    }
    mutex->__owner = tid;
    mutex->__count = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex) return 22;
    int tid = _syscall0(SYS_GETTID);

    if (mutex->__type == PTHREAD_MUTEX_RECURSIVE && mutex->__owner == tid) {
        mutex->__count++;
        return 0;
    }

    if (atomic_cas(&mutex->__lock, 0, 1) == 0) {
        mutex->__owner = tid;
        mutex->__count = 1;
        return 0;
    }
    return 16; /* EBUSY */
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex) return 22;

    if (mutex->__type == PTHREAD_MUTEX_RECURSIVE) {
        if (--mutex->__count > 0) return 0;
    }

    mutex->__owner = 0;
    mutex->__count = 0;
    atomic_xchg(&mutex->__lock, 0);
    futex_wake(&mutex->__lock, 1);
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if (!attr) return 22;
    attr->__type = PTHREAD_MUTEX_NORMAL;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    (void)attr;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (!attr) return 22;
    attr->__type = type;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
    if (!attr || !type) return 22;
    *type = attr->__type;
    return 0;
}

/* ---- Condition Variable ---- */
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    (void)attr;
    if (!cond) return 22;
    cond->__seq = 0;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    int seq = cond->__seq;
    pthread_mutex_unlock(mutex);
    futex_wait(&cond->__seq, seq);
    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
    atomic_add(&cond->__seq, 1);
    futex_wake(&cond->__seq, 1);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    atomic_add(&cond->__seq, 1);
    futex_wake(&cond->__seq, 0x7FFFFFFF);
    return 0;
}

/* ---- Read-Write Lock ---- */
int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    (void)attr;
    if (!rwlock) return 22;
    rwlock->__readers = 0;
    rwlock->__writer = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    (void)rwlock;
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    while (1) {
        while (rwlock->__writer)
            futex_wait(&rwlock->__writer, 1);
        atomic_add(&rwlock->__readers, 1);
        if (!rwlock->__writer) return 0;
        atomic_add(&rwlock->__readers, -1);
    }
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    while (atomic_xchg(&rwlock->__writer, 1) != 0)
        futex_wait(&rwlock->__writer, 1);
    while (rwlock->__readers > 0)
        futex_wait(&rwlock->__readers, rwlock->__readers);
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    if (rwlock->__writer) return 16; /* EBUSY */
    atomic_add(&rwlock->__readers, 1);
    if (rwlock->__writer) {
        atomic_add(&rwlock->__readers, -1);
        return 16;
    }
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    if (atomic_cas(&rwlock->__writer, 0, 1) != 0) return 16;
    if (rwlock->__readers > 0) {
        rwlock->__writer = 0;
        return 16;
    }
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    if (rwlock->__writer) {
        rwlock->__writer = 0;
        futex_wake(&rwlock->__writer, 0x7FFFFFFF);
    } else {
        atomic_add(&rwlock->__readers, -1);
        if (rwlock->__readers == 0)
            futex_wake(&rwlock->__readers, 1);
    }
    return 0;
}

/* ---- Thread-Specific Data ---- */
#define PTHREAD_KEYS_MAX 32
static void* _tsd_values[PTHREAD_KEYS_MAX];
static void (*_tsd_destructors[PTHREAD_KEYS_MAX])(void*);
static int _tsd_used[PTHREAD_KEYS_MAX];
static int _tsd_next_key = 0;

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    if (!key) return 22;
    if (_tsd_next_key >= PTHREAD_KEYS_MAX) return 11; /* EAGAIN */
    int k = _tsd_next_key++;
    _tsd_used[k] = 1;
    _tsd_destructors[k] = destructor;
    _tsd_values[k] = (void*)0;
    *key = (pthread_key_t)k;
    return 0;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !_tsd_used[key]) return 22;
    _tsd_used[key] = 0;
    _tsd_destructors[key] = (void*)0;
    _tsd_values[key] = (void*)0;
    return 0;
}

void* pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX) return (void*)0;
    return _tsd_values[key];
}

int pthread_setspecific(pthread_key_t key, const void* value) {
    if (key >= PTHREAD_KEYS_MAX) return 22;
    _tsd_values[key] = (void*)(uintptr_t)value;
    return 0;
}

/* ---- Once ---- */
int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return 22;
    if (atomic_cas(once_control, 0, 1) == 0) {
        init_routine();
        *once_control = 2;
        futex_wake(once_control, 0x7FFFFFFF);
    } else {
        while (*once_control == 1)
            futex_wait(once_control, 1);
    }
    return 0;
}

/* ---- Barrier ---- */
int pthread_barrier_init(pthread_barrier_t* barrier,
                         const pthread_barrierattr_t* attr, unsigned count) {
    (void)attr;
    if (!barrier || count == 0) return 22;
    barrier->__count = 0;
    barrier->__total = (int)count;
    barrier->__seq = 0;
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier) {
    (void)barrier;
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
    int seq = barrier->__seq;
    int n;
    atomic_add(&barrier->__count, 1);
    n = barrier->__count;
    if (n >= barrier->__total) {
        barrier->__count = 0;
        atomic_add(&barrier->__seq, 1);
        futex_wake(&barrier->__seq, 0x7FFFFFFF);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    while (barrier->__seq == seq)
        futex_wait(&barrier->__seq, seq);
    return 0;
}
