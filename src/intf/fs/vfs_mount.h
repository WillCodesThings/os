#pragma once 

#include <stdint.h>
#include <disk/partition.h>
#include <disk/block_device.h>

void vfs_mount_init(void);
int vfs_mount_partition(const char *mount_path, uint8_t drive, uint8_t partition_index, const char *fs_type);
int vfs_unmount(const char *mount_path);
partition_info_t *vfs_get_partition_for_path(const char *path);
block_device_t *vfs_get_block_device_for_path(const char *path);
void vfs_list_mounts(void);
