#ifndef TMPFS_H
#define TMPFS_H

#include "fs.h"
#include <stdint.h>

fs_node_t* tmpfs_create_root(void);
int tmpfs_add_file(fs_node_t* root_dir, const char* name, const uint8_t* data, uint32_t len);

#endif
