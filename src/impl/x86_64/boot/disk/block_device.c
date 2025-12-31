#include <disk/block_device.h>
#include <disk/ATA.h>

static int ata_read_block(struct block_device *dev, uint32_t block_num, uint8_t *buffer)
{
    if (!dev || !buffer)
        return -1;
    ata_read_sector(block_num, buffer);
    return 0;
}

static int ata_write_block(struct block_device *dev, uint32_t block_num, uint8_t *buffer)
{
    if (!dev || !buffer)
        return -1;
    ata_write_sector(block_num, buffer);
    return 0;
}

block_device_t ata_block_device = {
    .block_size = 512,
    .read_block = ata_read_block,
    .write_block = ata_write_block
};

block_device_t *get_block_device(void)
{
    return &ata_block_device;
}

