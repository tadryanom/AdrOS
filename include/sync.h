#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "spinlock.h"

/* ------------------------------------------------------------------ */
/* Kernel counting semaphore (blocking, sleep/wake — NOT spin-wait)   */
/* ------------------------------------------------------------------ */

#define KSEM_MAX_WAITERS 16

struct process; /* forward */

typedef struct ksem {
    spinlock_t    lock;
    int32_t       count;
    struct process* waiters[KSEM_MAX_WAITERS];
    uint32_t      nwaiters;
} ksem_t;

void ksem_init(ksem_t* s, int32_t initial_count);
void ksem_wait(ksem_t* s);

/* Wait with timeout (milliseconds). 0 = wait forever.
 * Returns 0 on success, 1 on timeout. */
int  ksem_wait_timeout(ksem_t* s, uint32_t timeout_ms);

void ksem_signal(ksem_t* s);

/* ------------------------------------------------------------------ */
/* Kernel mutex (binary semaphore)                                    */
/* ------------------------------------------------------------------ */

typedef struct kmutex {
    ksem_t sem;
} kmutex_t;

void kmutex_init(kmutex_t* m);
void kmutex_lock(kmutex_t* m);
void kmutex_unlock(kmutex_t* m);

/* ------------------------------------------------------------------ */
/* Kernel mailbox (fixed-size circular queue + semaphores)             */
/* ------------------------------------------------------------------ */

#define KMBOX_MAX_MSGS 32

typedef struct kmbox {
    void*    msgs[KMBOX_MAX_MSGS];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
    ksem_t   not_empty;
    ksem_t   not_full;
    spinlock_t lock;
} kmbox_t;

int  kmbox_init(kmbox_t* mb, uint32_t size);
void kmbox_free(kmbox_t* mb);
void kmbox_post(kmbox_t* mb, void* msg);
int  kmbox_trypost(kmbox_t* mb, void* msg);

/* Fetch with timeout (ms). 0 = wait forever.
 * Returns 0 on success, 1 on timeout. */
int  kmbox_fetch(kmbox_t* mb, void** msg, uint32_t timeout_ms);
int  kmbox_tryfetch(kmbox_t* mb, void** msg);

/* ------------------------------------------------------------------ */
/* Kernel condition variable (paired with kmutex_t)                    */
/* ------------------------------------------------------------------ */

#define KCOND_MAX_WAITERS 16

typedef struct kcond {
    spinlock_t      lock;
    struct process* waiters[KCOND_MAX_WAITERS];
    uint32_t        nwaiters;
} kcond_t;

void kcond_init(kcond_t* cv);

/* Release mutex, sleep until signaled, re-acquire mutex.
 * Returns 0 on success, 1 on timeout (timeout_ms == 0 means wait forever). */
int  kcond_wait(kcond_t* cv, kmutex_t* mtx, uint32_t timeout_ms);

/* Wake one waiter. */
void kcond_signal(kcond_t* cv);

/* Wake all waiters. */
void kcond_broadcast(kcond_t* cv);

#endif /* SYNC_H */
