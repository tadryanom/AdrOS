#include "kaslr.h"
#include "uart_console.h"

static uint32_t prng_state;

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void kaslr_init(void) {
    uint64_t tsc = rdtsc();
    prng_state = (uint32_t)(tsc ^ (tsc >> 32));
    if (prng_state == 0) prng_state = 0xDEADBEEF;
    uart_print("[KASLR] PRNG seeded from TSC\n");
}

uint32_t kaslr_rand(void) {
    /* xorshift32 */
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

uint32_t kaslr_offset(uint32_t max_pages) {
    if (max_pages == 0) return 0;
    return (kaslr_rand() % max_pages) * 0x1000U;
}
