#ifndef TMPFS_H
#define TMPFS_H

#include <fs/vfs.h>

// Initialize tmpfs and mount it as root
void tmpfs_init(void);

// Create a tmpfs file or directory
vfs_node_t *tmpfs_create_file(vfs_node_t *parent, const char *name, uint32_t flags);

#endif