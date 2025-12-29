#pragma once
#include <stdint.h>

typedef int (*read_block)(struct block_device *dev, uint32_t block_num, uint8_t *buffer);
typedef int (*write_block)(struct block_device *dev, uint32_t block_num, uint8_t *buffer);

typedef struct
{
    uint32_t block_size; // Size of each block in bytes
    read_block read_block;
    write_block write_block;
} block_device_t;