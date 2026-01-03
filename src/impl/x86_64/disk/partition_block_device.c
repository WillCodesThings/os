#include <disk/partition_block_device.h>
#include <disk/partition.h>
#include <disk/block_device.h>
#include <memory/heap.h>

typedef struct {
    block_device_t base;
    partition_info_t *partition;
} partition_block_device_t;

static int partition_block_read(struct block_device *dev, uint32_t block_num, uint8_t *buffer)
{
    if (!dev || !buffer)
        return -1;
    
    partition_block_device_t *part_dev = (partition_block_device_t *)dev;
    return partition_read(part_dev->partition, block_num, buffer);
}

static int partition_block_write(struct block_device *dev, uint32_t block_num, uint8_t *buffer)
{
    if (!dev || !buffer)
        return -1;
    
    partition_block_device_t *part_dev = (partition_block_device_t *)dev;
    return partition_write(part_dev->partition, block_num, buffer);
}

block_device_t *partition_create_block_device(partition_info_t *partition)
{
    if (!partition)
        return NULL;
    
    partition_block_device_t *part_dev = (partition_block_device_t *)kmalloc(sizeof(partition_block_device_t));
    if (!part_dev)
        return NULL;
    
    part_dev->base.block_size = 512;
    part_dev->base.read_block = partition_block_read;
    part_dev->base.write_block = partition_block_write;
    part_dev->partition = partition;
    
    return (block_device_t *)part_dev;
}