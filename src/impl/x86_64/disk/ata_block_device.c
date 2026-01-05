#include <disk/ata_block_device.h>
#include <interrupts/io/ata.h>
#include <memory/heap.h>
#include <shell/print.h>

int ata_block_read(struct block_device *dev, uint32_t lba, uint8_t *buffer)
{

    // serial_print("Reading sector ");
    // serial_print_hex(lba);
    // serial_print("\n");
    ata_block_device_t *ata_dev = (ata_block_device_t *)dev;
    return ata_read_sectors(ata_dev->drive, lba, 1, buffer);
}

int ata_block_write(struct block_device *dev, uint32_t lba, uint8_t *buffer)
{
    ata_block_device_t *ata_dev = (ata_block_device_t *)dev;
    return ata_write_sectors(ata_dev->drive, lba, 1, buffer);
}

block_device_t *ata_create_block_device(uint8_t drive)
{
    ata_block_device_t *dev = kmalloc(sizeof(ata_block_device_t));
    if (!dev)
        return NULL;

    dev->base.block_size = 512;
    dev->base.read_block = ata_block_read;
    dev->base.write_block = ata_block_write;
    dev->drive = drive;

    return &dev->base;
}
