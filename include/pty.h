#ifndef PTY_H
#define PTY_H

#include <stdint.h>
#include "fs.h"

#define PTY_MAX_PAIRS    8
#define PTY_MASTER_INO_BASE 100
#define PTY_SLAVE_INO_BASE  200

void pty_init(void);

int pty_alloc_pair(void);
int pty_pair_count(void);
int pty_pair_active(int idx);

int pty_master_read_kbuf(void* kbuf, uint32_t len);
int pty_master_write_kbuf(const void* kbuf, uint32_t len);
int pty_slave_read_kbuf(void* kbuf, uint32_t len);
int pty_slave_write_kbuf(const void* kbuf, uint32_t len);
int pty_master_can_read(void);
int pty_master_can_write(void);
int pty_slave_can_read(void);
int pty_slave_can_write(void);
int pty_slave_ioctl(uint32_t cmd, void* user_arg);

int pty_master_read_idx(int idx, void* kbuf, uint32_t len);
int pty_master_write_idx(int idx, const void* kbuf, uint32_t len);
int pty_slave_read_idx(int idx, void* kbuf, uint32_t len);
int pty_slave_write_idx(int idx, const void* kbuf, uint32_t len);
int pty_master_can_read_idx(int idx);
int pty_master_can_write_idx(int idx);
int pty_slave_can_read_idx(int idx);
int pty_slave_can_write_idx(int idx);
int pty_slave_ioctl_idx(int idx, uint32_t cmd, void* user_arg);

fs_node_t* pty_get_master_node(int idx);
fs_node_t* pty_get_slave_node(int idx);

static inline int pty_is_master_ino(uint32_t ino) {
    return (ino >= PTY_MASTER_INO_BASE && ino < PTY_MASTER_INO_BASE + PTY_MAX_PAIRS);
}
static inline int pty_is_slave_ino(uint32_t ino) {
    return (ino >= PTY_SLAVE_INO_BASE && ino < PTY_SLAVE_INO_BASE + PTY_MAX_PAIRS);
}
static inline int pty_ino_to_idx(uint32_t ino) {
    if (pty_is_master_ino(ino)) return (int)(ino - PTY_MASTER_INO_BASE);
    if (pty_is_slave_ino(ino)) return (int)(ino - PTY_SLAVE_INO_BASE);
    return -1;
}

#endif
