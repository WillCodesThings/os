#include <disk/ATA.h>
#include <interrupts/port_io.h>

static void ata_wait_busy()
{
    // Wait until BSY bit is cleared
    while (inb(ATA_STATUS) & 0x80)
        ;
}

static void ata_wait_drq()
{
    // Wait until DRQ bit is set
    while (!(inb(ATA_STATUS) & 0x08))
        ;
}

void ata_read_sector(uint32_t lba, uint8_t *buffer)
{
    ata_wait_busy();

    // Set up parameters
    outb(ATA_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F)); // LBA mode, master drive
    outb(ATA_SECTOR_COUNT0, 1);                          // Read 1 sector
    outb(ATA_LBA0, (uint8_t)(lba & 0xFF));               // LBA low byte
    outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xFF));        // LBA mid byte
    outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xFF));       // LBA high byte

    // Send read command
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    ata_wait_drq();

    // Read data (256 words = 512 bytes)
    for (int i = 0; i < 256; i++)
    {
        uint16_t data = inb(ATA_DATA) | (inb(ATA_DATA) << 8);
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }
}

void ata_write_sector(uint32_t lba, uint8_t *buffer)
{
    ata_wait_busy();

    // Set up parameters
    outb(ATA_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F)); // LBA mode, master drive
    outb(ATA_SECTOR_COUNT0, 1);                          // Write 1 sector
    outb(ATA_LBA0, (uint8_t)(lba & 0xFF));               // LBA low byte
    outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xFF));        // LBA mid byte
    outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xFF));       // LBA high byte

    // Send write command
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    ata_wait_drq();

    // Write data (256 words = 512 bytes)
    for (int i = 0; i < 256; i++)
    {
        uint16_t data = (buffer[i * 2] & 0xFF) | ((buffer[i * 2 + 1] & 0xFF) << 8);
        outb(ATA_DATA, data); // Send data
    }

    outb(ATA_DATA, 0); // Dummy write to complete the transfer
}
