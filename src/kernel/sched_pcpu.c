#include "sched_pcpu.h"
#include "console.h"

static uint32_t pcpu_load[SCHED_PCPU_MAX];
static uint32_t pcpu_count;

void sched_pcpu_init(uint32_t ncpus) {
    if (ncpus > SCHED_PCPU_MAX) ncpus = SCHED_PCPU_MAX;
    pcpu_count = ncpus;
    for (uint32_t i = 0; i < SCHED_PCPU_MAX; i++)
        pcpu_load[i] = 0;
    kprintf("[SCHED] Per-CPU runqueues initialized for %u CPU(s).\n",
            (unsigned)ncpus);
}

uint32_t sched_pcpu_count(void) {
    return pcpu_count;
}

uint32_t sched_pcpu_get_load(uint32_t cpu) {
    if (cpu >= pcpu_count) return 0;
    return __atomic_load_n(&pcpu_load[cpu], __ATOMIC_RELAXED);
}

uint32_t sched_pcpu_least_loaded(void) {
    uint32_t best = 0;
    uint32_t best_load = __atomic_load_n(&pcpu_load[0], __ATOMIC_RELAXED);
    for (uint32_t i = 1; i < pcpu_count; i++) {
        uint32_t l = __atomic_load_n(&pcpu_load[i], __ATOMIC_RELAXED);
        if (l < best_load) {
            best_load = l;
            best = i;
        }
    }
    return best;
}

void sched_pcpu_inc_load(uint32_t cpu) {
    if (cpu >= pcpu_count) return;
    __atomic_add_fetch(&pcpu_load[cpu], 1, __ATOMIC_RELAXED);
}

void sched_pcpu_dec_load(uint32_t cpu) {
    if (cpu >= pcpu_count) return;
    uint32_t old = __atomic_load_n(&pcpu_load[cpu], __ATOMIC_RELAXED);
    if (old > 0)
        __atomic_sub_fetch(&pcpu_load[cpu], 1, __ATOMIC_RELAXED);
}
