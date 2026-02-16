/*
 * AdrOS Rump Kernel Hypercall Implementation
 *
 * This file implements the rumpuser(3) hypercall interface, mapping
 * NetBSD Rump Kernel abstractions to AdrOS kernel primitives.
 *
 * Reference: https://man.netbsd.org/rumpuser.3
 *
 * Phase 1: Core (memory, console, init, params, random)
 * Phase 2: Threads + synchronization (TODO)
 * Phase 3: Clocks + signals (TODO)
 * Phase 4: File/Block I/O (TODO)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "heap.h"
#include "console.h"
#include "timer.h"
#include "process.h"
#include "sync.h"
#include "hal/cpu.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Rump hypercall version                                              */
/* ------------------------------------------------------------------ */

#define RUMPUSER_VERSION 17

/* ------------------------------------------------------------------ */
/* Upcall pointers (set by rumpuser_init)                              */
/* ------------------------------------------------------------------ */

typedef void (*rump_schedule_fn)(void);
typedef void (*rump_unschedule_fn)(void);

static rump_schedule_fn   g_hyp_schedule   = NULL;
static rump_unschedule_fn g_hyp_unschedule = NULL;

/* ------------------------------------------------------------------ */
/* Phase 1: Initialization                                             */
/* ------------------------------------------------------------------ */

struct rump_hyperup {
    rump_schedule_fn   hyp_schedule;
    rump_unschedule_fn hyp_unschedule;
    /* additional upcalls omitted for now */
};

int rumpuser_init(int version, const struct rump_hyperup *hyp) {
    if (version != RUMPUSER_VERSION) {
        kprintf("[RUMP] Version mismatch: kernel=%d, expected=%d\n",
                version, RUMPUSER_VERSION);
        return 1;
    }

    if (hyp) {
        g_hyp_schedule   = hyp->hyp_schedule;
        g_hyp_unschedule = hyp->hyp_unschedule;
    }

    kprintf("[RUMP] Hypercall layer initialized (v%d).\n", version);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Phase 1: Memory allocation                                          */
/* ------------------------------------------------------------------ */

int rumpuser_malloc(size_t len, int alignment, void **memp) {
    if (!memp) return 22; /* EINVAL */
    if (len == 0) { *memp = NULL; return 0; }

    /* kmalloc returns 16-byte aligned; for larger alignment, over-allocate */
    if (alignment <= 16) {
        *memp = kmalloc(len);
    } else {
        /* Over-allocate and manually align */
        size_t total = len + (size_t)alignment + sizeof(void*);
        void *raw = kmalloc(total);
        if (!raw) { *memp = NULL; return 12; } /* ENOMEM */
        uintptr_t addr = ((uintptr_t)raw + sizeof(void*) + (size_t)alignment - 1)
                         & ~((uintptr_t)alignment - 1);
        ((void**)addr)[-1] = raw;
        *memp = (void*)addr;
        return 0;
    }

    return *memp ? 0 : 12; /* ENOMEM */
}

void rumpuser_free(void *mem, size_t len) {
    (void)len;
    if (mem) kfree(mem);
}

/* ------------------------------------------------------------------ */
/* Phase 1: Console output                                             */
/* ------------------------------------------------------------------ */

void rumpuser_putchar(int ch) {
    char c = (char)ch;
    (void)c;
    kprintf("%c", ch);
}

void rumpuser_dprintf(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    kprintf("%s", buf);
}

/* ------------------------------------------------------------------ */
/* Phase 1: Termination                                                */
/* ------------------------------------------------------------------ */

#define RUMPUSER_PANIC 0xFF

void rumpuser_exit(int value) {
    if (value == RUMPUSER_PANIC) {
        kprintf("[RUMP] PANIC — halting.\n");
    } else {
        kprintf("[RUMP] Exit with code %d.\n", value);
    }
    for (;;) hal_cpu_idle();
}

/* ------------------------------------------------------------------ */
/* Phase 1: Parameter retrieval                                        */
/* ------------------------------------------------------------------ */

int rumpuser_getparam(const char *name, void *buf, size_t buflen) {
    if (!name || !buf || buflen == 0) return 22; /* EINVAL */

    if (strcmp(name, "_RUMPUSER_NCPU") == 0) {
        /* Report 1 CPU for now — SMP rump requires more work */
        strncpy((char*)buf, "1", buflen);
        return 0;
    }
    if (strcmp(name, "_RUMPUSER_HOSTNAME") == 0) {
        strncpy((char*)buf, "adros-rump", buflen);
        return 0;
    }
    if (strcmp(name, "RUMP_VERBOSE") == 0) {
        strncpy((char*)buf, "1", buflen);
        return 0;
    }

    /* Unknown parameter */
    ((char*)buf)[0] = '\0';
    return 2; /* ENOENT */
}

/* ------------------------------------------------------------------ */
/* Phase 1: Random                                                     */
/* ------------------------------------------------------------------ */

#define RUMPUSER_RANDOM_HARD   0x01
#define RUMPUSER_RANDOM_NOWAIT 0x02

int rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp) {
    (void)flags;
    if (!buf || !retp) return 22;

    /* Simple PRNG seeded from TSC — adequate for early bring-up */
    uint8_t *p = (uint8_t*)buf;
    uint32_t seed = (uint32_t)clock_gettime_ns();
    for (size_t i = 0; i < buflen; i++) {
        seed = seed * 1103515245 + 12345;
        p[i] = (uint8_t)(seed >> 16);
    }
    *retp = buflen;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Phase 3: Clocks                                                     */
/* ------------------------------------------------------------------ */

#define RUMPUSER_CLOCK_RELWALL 0
#define RUMPUSER_CLOCK_ABSMONO 1

extern uint32_t rtc_unix_timestamp(void);

int rumpuser_clock_gettime(int clk, int64_t *sec, long *nsec) {
    if (!sec || !nsec) return 22;

    if (clk == RUMPUSER_CLOCK_RELWALL) {
        *sec = (int64_t)rtc_unix_timestamp();
        *nsec = 0;
    } else {
        uint64_t ns = clock_gettime_ns();
        *sec = (int64_t)(ns / 1000000000ULL);
        *nsec = (long)(ns % 1000000000ULL);
    }
    return 0;
}

int rumpuser_clock_sleep(int clk, int64_t sec, long nsec) {
    if (clk == RUMPUSER_CLOCK_RELWALL) {
        uint32_t ms = (uint32_t)(sec * 1000 + nsec / 1000000);
        uint32_t ticks = (ms + TIMER_MS_PER_TICK - 1) / TIMER_MS_PER_TICK;
        if (ticks > 0) process_sleep(ticks);
    } else {
        /* ABSMONO: sleep until absolute monotonic time */
        uint64_t target_ns = (uint64_t)sec * 1000000000ULL + (uint64_t)nsec;
        uint64_t now = clock_gettime_ns();
        if (target_ns > now) {
            uint64_t delta_ms = (target_ns - now) / 1000000ULL;
            uint32_t ticks = (uint32_t)((delta_ms + TIMER_MS_PER_TICK - 1) / TIMER_MS_PER_TICK);
            if (ticks > 0) process_sleep(ticks);
        }
    }
    return 0;
}
