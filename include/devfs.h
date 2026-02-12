#ifndef DEVFS_H
#define DEVFS_H

#include "fs.h"

#define DEVFS_MAX_DEVICES 32

fs_node_t* devfs_create_root(void);

/*
 * Register a device node with devfs.
 * The caller owns the fs_node_t storage (must remain valid for the
 * lifetime of the device).  DevFS stores only a pointer.
 * Returns 0 on success, -1 if the registry is full.
 */
int devfs_register_device(fs_node_t *node);

#endif
