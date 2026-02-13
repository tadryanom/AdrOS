/*
 * HAL Device Driver Registry
 *
 * Maintains a table of registered drivers and provides lifecycle
 * management (probe → init → shutdown) in priority order.
 */
#include "hal/driver.h"
#include "console.h"
#include "utils.h"

static const struct hal_driver* drivers[HAL_MAX_DRIVERS];
static int driver_count = 0;

/* Track which drivers were successfully initialised (for shutdown order) */
static int driver_inited[HAL_MAX_DRIVERS];

int hal_driver_register(const struct hal_driver* drv) {
    if (!drv || !drv->name) return -1;
    if (driver_count >= HAL_MAX_DRIVERS) {
        kprintf("[DRV] registry full, cannot register '%s'\n", drv->name);
        return -1;
    }
    drivers[driver_count] = drv;
    driver_inited[driver_count] = 0;
    driver_count++;
    return 0;
}

/* Simple insertion sort by priority (stable, in-place on the pointer array) */
static void sort_drivers(void) {
    for (int i = 1; i < driver_count; i++) {
        const struct hal_driver* tmp = drivers[i];
        int j = i - 1;
        while (j >= 0 && drivers[j]->priority > tmp->priority) {
            drivers[j + 1] = drivers[j];
            j--;
        }
        drivers[j + 1] = tmp;
    }
}

int hal_drivers_init_all(void) {
    sort_drivers();

    int ok = 0;
    for (int i = 0; i < driver_count; i++) {
        const struct hal_driver* d = drivers[i];

        /* Probe: skip if hardware not present */
        if (d->ops.probe) {
            if (d->ops.probe() != 0) {
                kprintf("[DRV] %s: not detected, skipping\n", d->name);
                continue;
            }
        }

        /* Init */
        if (d->ops.init) {
            int rc = d->ops.init();
            if (rc != 0) {
                kprintf("[DRV] %s: init failed (%d)\n", d->name, rc);
                continue;
            }
        }

        driver_inited[i] = 1;
        ok++;
    }

    return ok;
}

void hal_drivers_shutdown_all(void) {
    /* Shutdown in reverse priority order */
    for (int i = driver_count - 1; i >= 0; i--) {
        if (driver_inited[i] && drivers[i]->ops.shutdown) {
            drivers[i]->ops.shutdown();
            driver_inited[i] = 0;
        }
    }
}

const struct hal_driver* hal_driver_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < driver_count; i++) {
        if (strcmp(drivers[i]->name, name) == 0)
            return drivers[i];
    }
    return NULL;
}

int hal_driver_count(void) {
    return driver_count;
}
