/*
 * lwIP sys_arch for AdrOS — NO_SYS=0 mode.
 * Provides semaphore, mutex, mailbox, thread, and protection primitives
 * backed by AdrOS kernel sync objects (include/sync.h).
 */
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/stats.h"

#include "sync.h"
#include "process.h"
#include "spinlock.h"

#include <stddef.h>

extern uint32_t get_tick_count(void);
extern void* kmalloc(uint32_t size);
extern void  kfree(void* ptr);
extern struct process* process_create_kernel(void (*entry)(void));

/* Return milliseconds since boot. Timer runs at 50 Hz → 20 ms per tick. */
u32_t sys_now(void) {
    return (u32_t)(get_tick_count() * 20);
}

/* ------------------------------------------------------------------ */
/* Semaphore                                                          */
/* ------------------------------------------------------------------ */

err_t sys_sem_new(sys_sem_t* sem, u8_t count) {
    if (!sem) return ERR_ARG;
    ksem_t* s = (ksem_t*)kmalloc(sizeof(ksem_t));
    if (!s) return ERR_MEM;
    ksem_init(s, (int32_t)count);
    *sem = s;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t* sem) {
    if (!sem || !*sem) return;
    kfree(*sem);
    *sem = NULL;
}

void sys_sem_signal(sys_sem_t* sem) {
    if (!sem || !*sem) return;
    ksem_signal(*sem);
}

u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout) {
    if (!sem || !*sem) return SYS_ARCH_TIMEOUT;
    u32_t start = sys_now();
    int rc = ksem_wait_timeout(*sem, timeout);
    if (rc != 0) return SYS_ARCH_TIMEOUT;
    u32_t elapsed = sys_now() - start;
    return elapsed;
}

/* ------------------------------------------------------------------ */
/* Mutex                                                              */
/* ------------------------------------------------------------------ */

err_t sys_mutex_new(sys_mutex_t* mutex) {
    if (!mutex) return ERR_ARG;
    kmutex_t* m = (kmutex_t*)kmalloc(sizeof(kmutex_t));
    if (!m) return ERR_MEM;
    kmutex_init(m);
    *mutex = m;
    return ERR_OK;
}

void sys_mutex_free(sys_mutex_t* mutex) {
    if (!mutex || !*mutex) return;
    kfree(*mutex);
    *mutex = NULL;
}

void sys_mutex_lock(sys_mutex_t* mutex) {
    if (!mutex || !*mutex) return;
    kmutex_lock(*mutex);
}

void sys_mutex_unlock(sys_mutex_t* mutex) {
    if (!mutex || !*mutex) return;
    kmutex_unlock(*mutex);
}

/* ------------------------------------------------------------------ */
/* Mailbox                                                            */
/* ------------------------------------------------------------------ */

err_t sys_mbox_new(sys_mbox_t* mbox, int size) {
    if (!mbox) return ERR_ARG;
    kmbox_t* mb = (kmbox_t*)kmalloc(sizeof(kmbox_t));
    if (!mb) return ERR_MEM;
    if (kmbox_init(mb, (uint32_t)(size > 0 ? size : KMBOX_MAX_MSGS)) < 0) {
        kfree(mb);
        return ERR_MEM;
    }
    *mbox = mb;
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t* mbox) {
    if (!mbox || !*mbox) return;
    kmbox_free(*mbox);
    kfree(*mbox);
    *mbox = NULL;
}

void sys_mbox_post(sys_mbox_t* mbox, void* msg) {
    if (!mbox || !*mbox) return;
    kmbox_post(*mbox, msg);
}

err_t sys_mbox_trypost(sys_mbox_t* mbox, void* msg) {
    if (!mbox || !*mbox) return ERR_ARG;
    if (kmbox_trypost(*mbox, msg) < 0) return ERR_MEM;
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t* mbox, void* msg) {
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** msg, u32_t timeout) {
    if (!mbox || !*mbox) return SYS_ARCH_TIMEOUT;
    u32_t start = sys_now();
    int rc = kmbox_fetch(*mbox, msg, timeout);
    if (rc != 0) return SYS_ARCH_TIMEOUT;
    u32_t elapsed = sys_now() - start;
    return elapsed;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void** msg) {
    if (!mbox || !*mbox) return SYS_MBOX_EMPTY;
    if (kmbox_tryfetch(*mbox, msg) < 0) return SYS_MBOX_EMPTY;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Thread                                                             */
/* ------------------------------------------------------------------ */

/* Wrapper: lwIP thread functions take a void* arg, but
 * process_create_kernel takes void (*)(void).
 * We store the real function + arg in a small struct and use a trampoline. */

struct lwip_thread_arg {
    lwip_thread_fn func;
    void*          arg;
};

#define LWIP_MAX_THREADS 4
static struct lwip_thread_arg lwip_thread_args[LWIP_MAX_THREADS];
static int lwip_thread_count = 0;

static void lwip_thread_trampoline_0(void) { lwip_thread_args[0].func(lwip_thread_args[0].arg); }
static void lwip_thread_trampoline_1(void) { lwip_thread_args[1].func(lwip_thread_args[1].arg); }
static void lwip_thread_trampoline_2(void) { lwip_thread_args[2].func(lwip_thread_args[2].arg); }
static void lwip_thread_trampoline_3(void) { lwip_thread_args[3].func(lwip_thread_args[3].arg); }

static void (*lwip_trampolines[LWIP_MAX_THREADS])(void) = {
    lwip_thread_trampoline_0,
    lwip_thread_trampoline_1,
    lwip_thread_trampoline_2,
    lwip_thread_trampoline_3,
};

sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread,
                            void* arg, int stacksize, int prio) {
    (void)name;
    (void)stacksize;
    (void)prio;

    if (lwip_thread_count >= LWIP_MAX_THREADS) return NULL;
    int idx = lwip_thread_count++;
    lwip_thread_args[idx].func = thread;
    lwip_thread_args[idx].arg = arg;

    struct process* p = process_create_kernel(lwip_trampolines[idx]);
    return (sys_thread_t)p;
}

/* ------------------------------------------------------------------ */
/* Critical section protection                                        */
/* ------------------------------------------------------------------ */

sys_prot_t sys_arch_protect(void) {
    return irq_save();
}

void sys_arch_unprotect(sys_prot_t pval) {
    irq_restore(pval);
}

/* ------------------------------------------------------------------ */
/* Init (called by lwIP)                                              */
/* ------------------------------------------------------------------ */

void sys_init(void) {
    /* Nothing to do — kernel primitives are already initialized */
}
