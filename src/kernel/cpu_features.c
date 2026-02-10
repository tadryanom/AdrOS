#include "hal/cpu_features.h"
#include "uart_console.h"

#include <stddef.h>

static struct cpu_features g_default_features;

__attribute__((weak))
void hal_cpu_detect_features(void) {
    for (size_t i = 0; i < sizeof(g_default_features); i++)
        ((uint8_t*)&g_default_features)[i] = 0;
    uart_print("[CPU] No arch-specific feature detection.\n");
}

__attribute__((weak))
const struct cpu_features* hal_cpu_get_features(void) {
    return &g_default_features;
}

__attribute__((weak))
void hal_cpu_print_features(void) {
    uart_print("[CPU] Feature detection not available.\n");
}
