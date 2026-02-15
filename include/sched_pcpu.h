#ifndef SCHED_PCPU_H
#define SCHED_PCPU_H

#include <stdint.h>

/*
 * Per-CPU scheduler runqueue infrastructure.
 *
 * Provides per-CPU runqueue data structures and load-balancing helpers.
 * The BSP currently runs the global scheduler; these structures prepare
 * the foundation for future AP scheduling.
 *
 * Usage:
 *   sched_pcpu_init()           — called once after SMP init
 *   sched_pcpu_get_load(cpu)    — query load on a CPU
 *   sched_pcpu_least_loaded()   — find CPU with fewest ready processes
 *   sched_pcpu_inc_load(cpu)    — increment load counter
 *   sched_pcpu_dec_load(cpu)    — decrement load counter
 */

#define SCHED_PCPU_MAX 16

void     sched_pcpu_init(uint32_t ncpus);
uint32_t sched_pcpu_get_load(uint32_t cpu);
uint32_t sched_pcpu_least_loaded(void);
void     sched_pcpu_inc_load(uint32_t cpu);
void     sched_pcpu_dec_load(uint32_t cpu);
uint32_t sched_pcpu_count(void);

#endif
