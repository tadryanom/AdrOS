/*
 * Virtio-blk PCI legacy driver.
 *
 * Implements a minimal virtio 0.9 (legacy) block device driver using
 * a single virtqueue for both reads and writes.  Uses PIO (port I/O)
 * for device configuration and a polling completion model.
 *
 * References:
 *   - Virtual I/O Device (VIRTIO) Version 1.0, §2 (legacy interface)
 *   - QEMU virtio-blk-pci device
 */
#include "virtio_blk.h"
#include "pci.h"
#include "console.h"
#include "utils.h"
#include "spinlock.h"
#include "pmm.h"
#include "vmm.h"
#include "interrupts.h"
#include "hal/driver.h"
#include "io.h"

#include <stddef.h>

#define KERNEL_VIRT_BASE 0xC0000000U
#define V2P(x) ((uintptr_t)(x) - KERNEL_VIRT_BASE)

/* ---- Virtio PCI legacy register offsets (from BAR0, I/O space) ---- */
#define VIRTIO_PCI_HOST_FEATURES    0x00
#define VIRTIO_PCI_GUEST_FEATURES   0x04
#define VIRTIO_PCI_QUEUE_PFN        0x08
#define VIRTIO_PCI_QUEUE_SIZE       0x0C  /* 16-bit */
#define VIRTIO_PCI_QUEUE_SEL        0x0E  /* 16-bit */
#define VIRTIO_PCI_QUEUE_NOTIFY     0x10  /* 16-bit */
#define VIRTIO_PCI_STATUS           0x12  /* 8-bit  */
#define VIRTIO_PCI_ISR              0x13  /* 8-bit  */
/* Device-specific config starts at offset 0x14 for legacy */
#define VIRTIO_PCI_BLK_CAPACITY     0x14  /* 64-bit: capacity in 512-byte sectors */

/* Virtio device status bits */
#define VIRTIO_STATUS_ACK           0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FAILED        0x80

/* Virtio descriptor flags */
#define VRING_DESC_F_NEXT     1
#define VRING_DESC_F_WRITE    2

/* Virtio-blk request types */
#define VIRTIO_BLK_T_IN   0  /* read */
#define VIRTIO_BLK_T_OUT  1  /* write */

/* ---- Vring structures ---- */
struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

/* Virtio-blk request header */
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

/* ---- Driver state ---- */
static uint16_t vblk_iobase;
static uint16_t vblk_queue_size;
static uint64_t vblk_capacity_sectors;
static int      vblk_ready;

static struct vring_desc*  vblk_desc;
static struct vring_avail* vblk_avail;
static struct vring_used*  vblk_used;
static uint16_t vblk_last_used_idx;

/* Statically allocated request header and status byte (DMA-accessible) */
static struct virtio_blk_req vblk_req_hdr __attribute__((aligned(16)));
static uint8_t vblk_status_byte __attribute__((aligned(4)));

static spinlock_t vblk_lock = {0};

/* ---- Vring size calculation (legacy) ---- */
static uint32_t vring_size(uint32_t num) {
    /* desc table + avail ring (aligned to page) + used ring */
    uint32_t s = num * (uint32_t)sizeof(struct vring_desc);
    s += sizeof(uint16_t) * (3 + num); /* avail: flags + idx + ring[num] + used_event */
    s = (s + 4095U) & ~4095U;
    s += sizeof(uint16_t) * 3 + num * (uint32_t)sizeof(struct vring_used_elem);
    return s;
}

/* ---- Init ---- */
int virtio_blk_init(void) {
    const struct pci_device* dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_BLK_DEVICE_ID);
    if (!dev) {
        kprintf("[VIRTIO-BLK] Device not found.\n");
        return -1;
    }

    /* BAR0 should be I/O space for legacy virtio */
    if (!(dev->bar[0] & 1)) {
        kprintf("[VIRTIO-BLK] BAR0 is MMIO, expected I/O.\n");
        return -1;
    }
    vblk_iobase = (uint16_t)(dev->bar[0] & 0xFFFCU);

    /* Enable PCI I/O space + bus mastering */
    uint32_t cmd = pci_config_read(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= (1U << 0) | (1U << 2); /* I/O Space | Bus Master */
    pci_config_write(dev->bus, dev->slot, dev->func, 0x04, cmd);

    /* Reset device */
    outb(vblk_iobase + VIRTIO_PCI_STATUS, 0);

    /* Acknowledge */
    outb(vblk_iobase + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK);
    outb(vblk_iobase + VIRTIO_PCI_STATUS,
              VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    /* Read host features, accept none for simplicity */
    (void)inl(vblk_iobase + VIRTIO_PCI_HOST_FEATURES);
    outl(vblk_iobase + VIRTIO_PCI_GUEST_FEATURES, 0);

    /* Read capacity */
    uint32_t cap_lo = inl(vblk_iobase + VIRTIO_PCI_BLK_CAPACITY);
    uint32_t cap_hi = inl(vblk_iobase + VIRTIO_PCI_BLK_CAPACITY + 4);
    vblk_capacity_sectors = ((uint64_t)cap_hi << 32) | cap_lo;

    /* Select queue 0 */
    outw(vblk_iobase + VIRTIO_PCI_QUEUE_SEL, 0);
    vblk_queue_size = inw(vblk_iobase + VIRTIO_PCI_QUEUE_SIZE);
    if (vblk_queue_size == 0) {
        kprintf("[VIRTIO-BLK] Queue size is 0.\n");
        outb(vblk_iobase + VIRTIO_PCI_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    /* Allocate vring pages */
    uint32_t total = vring_size(vblk_queue_size);
    uint32_t pages = (total + 4095U) / 4096U;

    /* Use a fixed VA range for virtio vring */
    #define VIRTIO_VRING_VA 0xC0340000U
    for (uint32_t i = 0; i < pages; i++) {
        void* frame = pmm_alloc_page();
        if (!frame) {
            kprintf("[VIRTIO-BLK] Failed to alloc vring page.\n");
            outb(vblk_iobase + VIRTIO_PCI_STATUS, VIRTIO_STATUS_FAILED);
            return -1;
        }
        vmm_map_page((uint64_t)(uintptr_t)frame,
                     (uint64_t)(VIRTIO_VRING_VA + i * 4096U),
                     VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_NOCACHE);
    }
    memset((void*)VIRTIO_VRING_VA, 0, pages * 4096U);

    /* Set up vring pointers */
    vblk_desc = (struct vring_desc*)VIRTIO_VRING_VA;
    uint32_t avail_off = vblk_queue_size * (uint32_t)sizeof(struct vring_desc);
    vblk_avail = (struct vring_avail*)(VIRTIO_VRING_VA + avail_off);
    uint32_t used_off = avail_off + sizeof(uint16_t) * (3 + vblk_queue_size);
    used_off = (used_off + 4095U) & ~4095U;
    vblk_used = (struct vring_used*)(VIRTIO_VRING_VA + used_off);
    vblk_last_used_idx = 0;

    /* Tell device where the vring lives (page-aligned physical address) */
    uint32_t vring_phys = (uint32_t)V2P(VIRTIO_VRING_VA);
    outl(vblk_iobase + VIRTIO_PCI_QUEUE_PFN, vring_phys / 4096U);

    /* Mark driver ready */
    outb(vblk_iobase + VIRTIO_PCI_STATUS,
              VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    vblk_ready = 1;
    kprintf("[VIRTIO-BLK] Initialized: %llu sectors (%llu MB), queue=%u, IO=0x%x\n",
            (unsigned long long)vblk_capacity_sectors,
            (unsigned long long)(vblk_capacity_sectors / 2048),
            (unsigned)vblk_queue_size,
            (unsigned)vblk_iobase);

    return 0;
}

/* ---- Submit a single request and poll for completion ---- */
static int vblk_do_request(uint32_t type, uint64_t sector,
                           void* buf, uint32_t bytes) {
    if (!vblk_ready) return -1;

    uintptr_t fl = spin_lock_irqsave(&vblk_lock);

    /* Set up request header */
    vblk_req_hdr.type = type;
    vblk_req_hdr.reserved = 0;
    vblk_req_hdr.sector = sector;
    vblk_status_byte = 0xFF;

    /* Descriptor 0: request header (device-readable) */
    vblk_desc[0].addr = (uint64_t)V2P((uintptr_t)&vblk_req_hdr);
    vblk_desc[0].len = sizeof(vblk_req_hdr);
    vblk_desc[0].flags = VRING_DESC_F_NEXT;
    vblk_desc[0].next = 1;

    /* Descriptor 1: data buffer */
    vblk_desc[1].addr = (uint64_t)V2P((uintptr_t)buf);
    vblk_desc[1].len = bytes;
    vblk_desc[1].flags = VRING_DESC_F_NEXT;
    if (type == VIRTIO_BLK_T_IN) {
        vblk_desc[1].flags |= VRING_DESC_F_WRITE; /* device writes to buffer */
    }
    vblk_desc[1].next = 2;

    /* Descriptor 2: status byte (device-writable) */
    vblk_desc[2].addr = (uint64_t)V2P((uintptr_t)&vblk_status_byte);
    vblk_desc[2].len = 1;
    vblk_desc[2].flags = VRING_DESC_F_WRITE;
    vblk_desc[2].next = 0;

    /* Add to available ring */
    uint16_t avail_idx = vblk_avail->idx;
    vblk_avail->ring[avail_idx % vblk_queue_size] = 0; /* head descriptor */
    __sync_synchronize();
    vblk_avail->idx = avail_idx + 1;
    __sync_synchronize();

    /* Notify device (queue 0) */
    outw(vblk_iobase + VIRTIO_PCI_QUEUE_NOTIFY, 0);

    /* Poll for completion */
    uint32_t spins = 0;
    while (vblk_used->idx == vblk_last_used_idx) {
        __sync_synchronize();
        if (++spins > 10000000U) {
            spin_unlock_irqrestore(&vblk_lock, fl);
            kprintf("[VIRTIO-BLK] Request timeout.\n");
            return -1;
        }
    }
    vblk_last_used_idx++;

    /* Read ISR to clear interrupt */
    (void)inb(vblk_iobase + VIRTIO_PCI_ISR);

    int ret = (vblk_status_byte == 0) ? 0 : -1;
    spin_unlock_irqrestore(&vblk_lock, fl);
    return ret;
}

int virtio_blk_read(uint64_t sector, void* buf, uint32_t count) {
    if (!buf || count == 0) return -1;
    for (uint32_t i = 0; i < count; i++) {
        int rc = vblk_do_request(VIRTIO_BLK_T_IN, sector + i,
                                 (uint8_t*)buf + i * 512, 512);
        if (rc < 0) return rc;
    }
    return 0;
}

int virtio_blk_write(uint64_t sector, const void* buf, uint32_t count) {
    if (!buf || count == 0) return -1;
    for (uint32_t i = 0; i < count; i++) {
        int rc = vblk_do_request(VIRTIO_BLK_T_OUT, sector + i,
                                 (void*)((uint8_t*)buf + i * 512), 512);
        if (rc < 0) return rc;
    }
    return 0;
}

uint64_t virtio_blk_capacity(void) {
    return vblk_capacity_sectors;
}

/* ---- HAL driver registration ---- */
static int vblk_drv_probe(void) {
    return pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_BLK_DEVICE_ID) ? 0 : -1;
}

static const struct hal_driver vblk_hal_driver = {
    .name     = "virtio-blk",
    .type     = HAL_DRV_BLOCK,
    .priority = 25,
    .ops = {
        .probe    = vblk_drv_probe,
        .init     = (int (*)(void))virtio_blk_init,
        .shutdown = NULL,
    },
};

void virtio_blk_driver_register(void) {
    hal_driver_register(&vblk_hal_driver);
}
