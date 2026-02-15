#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#include <stdint.h>

/* Virtio PCI legacy device IDs */
#define VIRTIO_VENDOR_ID     0x1AF4
#define VIRTIO_BLK_DEVICE_ID 0x1001   /* transitional virtio-blk */

int  virtio_blk_init(void);
int  virtio_blk_read(uint64_t sector, void* buf, uint32_t count);
int  virtio_blk_write(uint64_t sector, const void* buf, uint32_t count);
uint64_t virtio_blk_capacity(void);

void virtio_blk_driver_register(void);

#endif
