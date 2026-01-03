#pragma once
#include <disk/block_device.h>

typedef struct {
    block_device_t base;
    uint8_t drive;   // ATA_PRIMARY_MASTER, etc.
} ata_block_device_t;

block_device_t *ata_create_block_device(uint8_t drive);

int ata_read_block(struct block_device *dev, uint32_t block_num, uint8_t *buffer);
int ata_write_block(struct block_device *dev, uint32_t block_num, uint8_t *buffer);

