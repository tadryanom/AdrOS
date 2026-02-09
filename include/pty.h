#ifndef PTY_H
#define PTY_H

#include <stdint.h>

void pty_init(void);

int pty_master_read_kbuf(void* kbuf, uint32_t len);
int pty_master_write_kbuf(const void* kbuf, uint32_t len);

int pty_slave_read_kbuf(void* kbuf, uint32_t len);
int pty_slave_write_kbuf(const void* kbuf, uint32_t len);

int pty_master_can_read(void);
int pty_master_can_write(void);

int pty_slave_can_read(void);
int pty_slave_can_write(void);

int pty_slave_ioctl(uint32_t cmd, void* user_arg);

#endif
