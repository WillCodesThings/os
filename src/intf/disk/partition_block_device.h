#pragma once

#include <disk/partition.h>
#include <disk/block_device.h>

block_device_t *partition_create_block_device(partition_info_t *partition);
