#pragma once
#include <stdint.h>

struct block_device; // forward declaration

typedef struct block_device
{
    uint32_t block_size;
    int (*read_block)(struct block_device *, uint32_t, uint8_t *);
    int (*write_block)(struct block_device *, uint32_t, uint8_t *);
} block_device_t;
