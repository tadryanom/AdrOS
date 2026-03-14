// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_PTHREAD_H
#define ULIBC_PTHREAD_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t pthread_t;

typedef struct {
    size_t stack_size;
    int    detach_state;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* Create a new thread.
 * thread: output thread ID
 * attr: thread attributes (may be NULL for defaults)
 * start_routine: function pointer
 * arg: argument passed to start_routine
 * Returns 0 on success, errno on failure.
 */
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);

/* Wait for a thread to finish.
 * thread: thread ID to wait for
 * retval: if non-NULL, stores the thread's return value
 * Returns 0 on success, errno on failure.
 */
int pthread_join(pthread_t thread, void** retval);

/* Terminate the calling thread.
 * retval: return value for pthread_join
 */
void pthread_exit(void* retval) __attribute__((noreturn));

/* Return the calling thread's ID. */
pthread_t pthread_self(void);

/* Initialize thread attributes to defaults. */
int pthread_attr_init(pthread_attr_t* attr);

/* Destroy thread attributes. */
int pthread_attr_destroy(pthread_attr_t* attr);

/* Set stack size in thread attributes. */
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);

/* Detach */
int pthread_detach(pthread_t thread);

/* Cancel */
int pthread_cancel(pthread_t thread);
int pthread_setcancelstate(int state, int* oldstate);
int pthread_setcanceltype(int type, int* oldtype);
void pthread_testcancel(void);

#define PTHREAD_CANCEL_ENABLE  0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED     0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1

/* ---- Mutex ---- */
typedef struct {
    volatile int __lock;
    int          __owner;
    int          __type;
    int          __count;
} pthread_mutex_t;

typedef struct {
    int __type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0, 0 }

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);

int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type);

/* ---- Condition Variable ---- */
typedef struct {
    volatile int __seq;
} pthread_cond_t;

typedef struct {
    int __dummy;
} pthread_condattr_t;

#define PTHREAD_COND_INITIALIZER { 0 }

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);

/* ---- Read-Write Lock ---- */
typedef struct {
    volatile int __readers;
    volatile int __writer;
} pthread_rwlock_t;

typedef struct {
    int __dummy;
} pthread_rwlockattr_t;

#define PTHREAD_RWLOCK_INITIALIZER { 0, 0 }

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);

/* ---- Thread-Specific Data (Keys) ---- */
typedef unsigned int pthread_key_t;

int   pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int   pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int   pthread_setspecific(pthread_key_t key, const void* value);

/* ---- Once ---- */
typedef volatile int pthread_once_t;
#define PTHREAD_ONCE_INIT 0

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

/* ---- Barrier (optional) ---- */
typedef struct {
    volatile int __count;
    volatile int __total;
    volatile int __seq;
} pthread_barrier_t;

typedef struct {
    int __dummy;
} pthread_barrierattr_t;

#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

int pthread_barrier_init(pthread_barrier_t* barrier,
                         const pthread_barrierattr_t* attr, unsigned count);
int pthread_barrier_destroy(pthread_barrier_t* barrier);
int pthread_barrier_wait(pthread_barrier_t* barrier);

#endif
