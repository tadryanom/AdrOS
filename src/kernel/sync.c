#include "sync.h"
#include "process.h"
#include "utils.h"
#include "timer.h"

extern uint32_t get_tick_count(void);
extern void schedule(void);
extern void sched_enqueue_ready(struct process* p);
extern struct process* current_process;

/* ------------------------------------------------------------------ */
/* Kernel Semaphore                                                   */
/* ------------------------------------------------------------------ */

void ksem_init(ksem_t* s, int32_t initial_count) {
    if (!s) return;
    spinlock_init(&s->lock);
    s->count = initial_count;
    s->nwaiters = 0;
    for (uint32_t i = 0; i < KSEM_MAX_WAITERS; i++)
        s->waiters[i] = 0;
}

void ksem_wait(ksem_t* s) {
    (void)ksem_wait_timeout(s, 0);
}

int ksem_wait_timeout(ksem_t* s, uint32_t timeout_ms) {
    if (!s) return 1;

    uintptr_t flags = spin_lock_irqsave(&s->lock);
    if (s->count > 0) {
        s->count--;
        spin_unlock_irqrestore(&s->lock, flags);
        return 0;
    }

    /* Need to block — add ourselves to the wait list */
    if (!current_process || s->nwaiters >= KSEM_MAX_WAITERS) {
        spin_unlock_irqrestore(&s->lock, flags);
        return 1;
    }

    s->waiters[s->nwaiters++] = current_process;
    current_process->state = PROCESS_BLOCKED;

    /* Set a wake timeout if requested (convert ms to ticks) */
    uint32_t deadline = 0;
    if (timeout_ms > 0) {
        uint32_t ticks = (timeout_ms + TIMER_MS_PER_TICK - 1) / TIMER_MS_PER_TICK;
        deadline = get_tick_count() + ticks;
        current_process->wake_at_tick = deadline;
        current_process->state = PROCESS_SLEEPING; /* timer will wake us */
    }

    spin_unlock_irqrestore(&s->lock, flags);
    schedule();

    /* We were woken — check if it was a timeout or a signal */
    flags = spin_lock_irqsave(&s->lock);

    /* Remove ourselves from waiters if still present (timeout case) */
    int found = 0;
    for (uint32_t i = 0; i < s->nwaiters; i++) {
        if (s->waiters[i] == current_process) {
            /* We timed out — remove from list */
            for (uint32_t j = i; j + 1 < s->nwaiters; j++)
                s->waiters[j] = s->waiters[j + 1];
            s->waiters[--s->nwaiters] = 0;
            found = 1;
            break;
        }
    }

    spin_unlock_irqrestore(&s->lock, flags);

    /* If we were still in the waiters list, it was a timeout */
    return found ? 1 : 0;
}

void ksem_signal(ksem_t* s) {
    if (!s) return;

    uintptr_t flags = spin_lock_irqsave(&s->lock);

    /* Find a waiter still blocked/sleeping (skip those already woken by timeout) */
    struct process* to_wake = NULL;
    for (uint32_t i = 0; i < s->nwaiters; i++) {
        struct process* p = s->waiters[i];
        if (p && (p->state == PROCESS_BLOCKED || p->state == PROCESS_SLEEPING)) {
            /* Remove from waiters list */
            for (uint32_t j = i; j + 1 < s->nwaiters; j++)
                s->waiters[j] = s->waiters[j + 1];
            s->waiters[--s->nwaiters] = 0;

            p->state = PROCESS_READY;
            p->wake_at_tick = 0;
            to_wake = p;
            break;
        }
    }

    if (!to_wake) {
        s->count++;
    }

    spin_unlock_irqrestore(&s->lock, flags);

    /* Enqueue outside the semaphore lock to avoid lock-order issues
     * (sched_enqueue_ready acquires sched_lock internally). */
    if (to_wake) {
        sched_enqueue_ready(to_wake);
    }
}

/* ------------------------------------------------------------------ */
/* Kernel Mutex                                                       */
/* ------------------------------------------------------------------ */

void kmutex_init(kmutex_t* m) {
    if (!m) return;
    ksem_init(&m->sem, 1);
}

void kmutex_lock(kmutex_t* m) {
    if (!m) return;
    ksem_wait(&m->sem);
}

void kmutex_unlock(kmutex_t* m) {
    if (!m) return;
    ksem_signal(&m->sem);
}

/* ------------------------------------------------------------------ */
/* Kernel Mailbox                                                     */
/* ------------------------------------------------------------------ */

int kmbox_init(kmbox_t* mb, uint32_t size) {
    if (!mb) return -1;
    if (size == 0 || size > KMBOX_MAX_MSGS) size = KMBOX_MAX_MSGS;

    spinlock_init(&mb->lock);
    mb->head = 0;
    mb->tail = 0;
    mb->count = 0;
    mb->capacity = size;
    for (uint32_t i = 0; i < KMBOX_MAX_MSGS; i++)
        mb->msgs[i] = 0;

    ksem_init(&mb->not_empty, 0);
    ksem_init(&mb->not_full, (int32_t)size);
    return 0;
}

void kmbox_free(kmbox_t* mb) {
    if (!mb) return;
    mb->count = 0;
    mb->head = 0;
    mb->tail = 0;
}

void kmbox_post(kmbox_t* mb, void* msg) {
    if (!mb) return;
    ksem_wait(&mb->not_full);

    uintptr_t flags = spin_lock_irqsave(&mb->lock);
    mb->msgs[mb->tail] = msg;
    mb->tail = (mb->tail + 1) % mb->capacity;
    mb->count++;
    spin_unlock_irqrestore(&mb->lock, flags);

    ksem_signal(&mb->not_empty);
}

int kmbox_trypost(kmbox_t* mb, void* msg) {
    if (!mb) return -1;

    uintptr_t flags = spin_lock_irqsave(&mb->lock);
    if (mb->count >= mb->capacity) {
        spin_unlock_irqrestore(&mb->lock, flags);
        return -1;
    }
    mb->msgs[mb->tail] = msg;
    mb->tail = (mb->tail + 1) % mb->capacity;
    mb->count++;
    spin_unlock_irqrestore(&mb->lock, flags);

    ksem_signal(&mb->not_empty);
    return 0;
}

int kmbox_fetch(kmbox_t* mb, void** msg, uint32_t timeout_ms) {
    if (!mb) return 1;

    int rc = ksem_wait_timeout(&mb->not_empty, timeout_ms);
    if (rc != 0) return 1; /* timeout */

    uintptr_t flags = spin_lock_irqsave(&mb->lock);
    void* m = mb->msgs[mb->head];
    mb->head = (mb->head + 1) % mb->capacity;
    mb->count--;
    spin_unlock_irqrestore(&mb->lock, flags);

    if (msg) *msg = m;

    ksem_signal(&mb->not_full);
    return 0;
}

int kmbox_tryfetch(kmbox_t* mb, void** msg) {
    if (!mb) return -1;

    uintptr_t flags = spin_lock_irqsave(&mb->lock);
    if (mb->count == 0) {
        spin_unlock_irqrestore(&mb->lock, flags);
        return -1;
    }
    void* m = mb->msgs[mb->head];
    mb->head = (mb->head + 1) % mb->capacity;
    mb->count--;
    spin_unlock_irqrestore(&mb->lock, flags);

    if (msg) *msg = m;

    ksem_signal(&mb->not_full);
    return 0;
}
