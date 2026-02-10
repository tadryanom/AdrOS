#ifndef SHM_H
#define SHM_H

#include <stdint.h>
#include <stddef.h>

#define SHM_MAX_SEGMENTS 32
#define SHM_MAX_PAGES    16   /* max pages per segment (64KB) */

/* Flags for shmget */
#define IPC_CREAT   0x0200
#define IPC_EXCL    0x0400

/* Commands for shmctl */
#define IPC_RMID    0
#define IPC_STAT    1

/* Private key — always creates a new segment */
#define IPC_PRIVATE 0

struct shmid_ds {
    uint32_t shm_segsz;     /* segment size in bytes */
    uint32_t shm_nattch;    /* number of current attaches */
    uint32_t shm_key;       /* key */
};

/* Kernel API */
int    shm_get(uint32_t key, uint32_t size, int flags);
void*  shm_at(int shmid, uintptr_t shmaddr);
int    shm_dt(const void* shmaddr);
int    shm_ctl(int shmid, int cmd, struct shmid_ds* buf);

void   shm_init(void);

#endif
