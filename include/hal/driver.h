#ifndef HAL_DRIVER_H
#define HAL_DRIVER_H

#include <stdint.h>
#include <stddef.h>

/*
 * HAL Device Driver Model
 *
 * Provides a unified registration and lifecycle interface for all
 * hardware drivers, both platform-specific (x86 PS/2, PIT, LAPIC)
 * and generic (ATA, E1000, VBE framebuffer).
 *
 * Architecture-dependent implementations register themselves at boot
 * via hal_driver_register().  The kernel init code calls
 * hal_drivers_init_all() to probe and initialise every registered
 * driver in priority order.
 */

/* Driver categories */
enum hal_driver_type {
    HAL_DRV_PLATFORM,     /* arch-specific: PIT, PS/2, LAPIC, IOAPIC */
    HAL_DRV_CHAR,         /* character devices: UART, keyboard, VGA text */
    HAL_DRV_BLOCK,        /* block devices: ATA, virtio-blk */
    HAL_DRV_NET,          /* network: E1000 */
    HAL_DRV_DISPLAY,      /* display: VBE framebuffer */
    HAL_DRV_BUS,          /* bus controllers: PCI */
    HAL_DRV_TYPE_COUNT
};

/* Driver operations table */
struct hal_driver_ops {
    int  (*probe)(void);      /* detect hardware; return 0 if present */
    int  (*init)(void);       /* initialise the driver; return 0 on success */
    void (*shutdown)(void);   /* graceful shutdown / cleanup */
};

/* Driver descriptor — typically declared as a const static in each driver */
struct hal_driver {
    const char*            name;
    enum hal_driver_type   type;
    int                    priority;  /* lower = earlier init (0-99) */
    struct hal_driver_ops  ops;
};

#define HAL_MAX_DRIVERS 32

/*
 * Register a driver with the HAL subsystem.
 * Must be called before hal_drivers_init_all().
 * Returns 0 on success, -1 if registry is full.
 */
int hal_driver_register(const struct hal_driver* drv);

/*
 * Probe and initialise all registered drivers in priority order.
 * Called once during kernel boot.
 * Returns the number of drivers successfully initialised.
 */
int hal_drivers_init_all(void);

/*
 * Shutdown all initialised drivers in reverse order.
 */
void hal_drivers_shutdown_all(void);

/*
 * Look up a registered driver by name.
 * Returns NULL if not found.
 */
const struct hal_driver* hal_driver_find(const char* name);

/*
 * Return the number of registered drivers.
 */
int hal_driver_count(void);

#endif /* HAL_DRIVER_H */
