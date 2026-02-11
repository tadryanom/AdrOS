#ifndef WAITQUEUE_H
#define WAITQUEUE_H

#include <stdint.h>
#include "process.h"

#define WQ_CAPACITY 16

typedef struct waitqueue {
    struct process* entries[WQ_CAPACITY];
    uint32_t head;
    uint32_t tail;
} waitqueue_t;

static inline void wq_init(waitqueue_t* wq) {
    wq->head = 0;
    wq->tail = 0;
}

static inline int wq_empty(const waitqueue_t* wq) {
    return wq->head == wq->tail;
}

static inline int wq_push(waitqueue_t* wq, struct process* p) {
    uint32_t next = (wq->head + 1U) % WQ_CAPACITY;
    if (next == wq->tail) return -1;
    wq->entries[wq->head] = p;
    wq->head = next;
    return 0;
}

static inline struct process* wq_pop(waitqueue_t* wq) {
    if (wq_empty(wq)) return NULL;
    struct process* p = wq->entries[wq->tail];
    wq->tail = (wq->tail + 1U) % WQ_CAPACITY;
    return p;
}

static inline void wq_wake_one(waitqueue_t* wq) {
    struct process* p = wq_pop(wq);
    if (p && p->state == PROCESS_BLOCKED) {
        p->state = PROCESS_READY;
        sched_enqueue_ready(p);
    }
}

static inline void wq_wake_all(waitqueue_t* wq) {
    while (!wq_empty(wq)) {
        wq_wake_one(wq);
    }
}

#endif
