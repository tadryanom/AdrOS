#include "kaslr.h"
#include "hal/cpu.h"
#include "console.h"

static uint32_t prng_state;

void kaslr_init(void) {
    uint64_t tsc = hal_cpu_read_timestamp();
    prng_state = (uint32_t)(tsc ^ (tsc >> 32));
    if (prng_state == 0) prng_state = 0xDEADBEEF;
    kprintf("[KASLR] PRNG seeded from TSC\n");
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
