#ifndef OVERLAYFS_H
#define OVERLAYFS_H

#include "fs.h"

fs_node_t* overlayfs_create_root(fs_node_t* lower_root, fs_node_t* upper_root);

#endif
