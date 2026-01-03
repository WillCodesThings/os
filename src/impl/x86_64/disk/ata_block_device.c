#include <disk/ata_block_device.h>
#include <interrupts/io/ata.h>
#include <memory/heap.h>
#include <shell/print.h>

int ata_block_read(uint8_t drive, uint32_t lba, uint16_t *buffer){
    serial_print("Reading sector ");
    serial_print_hex(lba);
    serial_print("\n");
    return ata_read_sectors(drive, lba, 1, buffer);  
}

int ata_block_write(uint8_t drive, uint32_t lba, uint16_t *buffer){
    serial_print("Writing sector ");
    serial_print_hex(lba);
    serial_print("\n");
    return ata_write_sectors(drive, lba, 1, buffer);  
}

block_device_t *ata_create_block_device(uint8_t drive)
{
    ata_block_device_t *dev = kmalloc(sizeof(ata_block_device_t));
    if (!dev)
        return NULL;

    dev->base.block_size  = 512;
    dev->base.read_block  = ata_block_read;
    dev->base.write_block = ata_block_write;
    dev->drive = drive;

    return &dev->base;
}
