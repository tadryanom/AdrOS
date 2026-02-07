#include "kernel/init.h"

#include "arch/arch_platform.h"

#include "fs.h"
#include "initrd.h"
#include "tty.h"
#include "uart_console.h"

#include "hal/mm.h"

#include <stddef.h>

static int cmdline_has_token(const char* cmdline, const char* token) {
    if (!cmdline || !token) return 0;

    for (size_t i = 0; cmdline[i] != 0; i++) {
        size_t j = 0;
        while (token[j] != 0 && cmdline[i + j] == token[j]) {
            j++;
        }
        if (token[j] == 0) {
            char before = (i == 0) ? ' ' : cmdline[i - 1];
            char after = cmdline[i + j];
            int before_ok = (before == ' ' || before == '\t');
            int after_ok = (after == 0 || after == ' ' || after == '\t');
            if (before_ok && after_ok) return 1;
        }
    }

    return 0;
}

int init_start(const struct boot_info* bi) {
    if (bi && bi->initrd_start) {
        uintptr_t initrd_virt = 0;
        if (hal_mm_map_physical_range((uintptr_t)bi->initrd_start, (uintptr_t)bi->initrd_end,
                                      HAL_MM_MAP_RW, &initrd_virt) == 0) {
            fs_root = initrd_init((uint32_t)initrd_virt);
        } else {
            uart_print("[INITRD] Failed to map initrd physical range.\n");
        }
    }

    tty_init();

    int user_ret = arch_platform_start_userspace(bi);

    if (bi && cmdline_has_token(bi->cmdline, "ring3")) {
        arch_platform_usermode_test_start();
    }

    return user_ret;
}
