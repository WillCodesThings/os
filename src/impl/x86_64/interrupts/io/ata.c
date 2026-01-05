#include <interrupts/io/ata.h>
#include <interrupts/idt.h>
#include <interrupts/port_io.h>
#include <shell/shell.h>
#include <shell/print.h>

// Forward declaration of assembly handler
extern void ata_primary_interrupt_handler(void);
extern void ata_secondary_interrupt_handler(void);

ata_device_t ata_primary = {0};
ata_device_t ata_secondary = {0};

// Wait for ATA device to be ready
static void ata_wait_ready(ata_device_t *dev)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY))
        {
            return;
        }
    }
}

static void ata_io_wait(ata_device_t *dev)
{
    inb(dev->control_base + ATA_REG_ALTSTATUS);
    inb(dev->control_base + ATA_REG_ALTSTATUS);
    inb(dev->control_base + ATA_REG_ALTSTATUS);
    inb(dev->control_base + ATA_REG_ALTSTATUS);
}

static int ata_wait_not_busy(ata_device_t *dev)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            return (status & ATA_SR_ERR) ? -1 : 0;
    }
    return -1;
}

static int ata_wait_drq(ata_device_t *dev)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR)
            return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
            return 0;
    }
    return -1;
}

// Read status without clearing interrupt
static uint8_t ata_read_altstatus(ata_device_t *dev)
{
    return inb(dev->control_base + ATA_REG_ALTSTATUS);
}

void ata_primary_handle_interrupt(void)
{
    // acknowledge interrupt
    ata_primary.status = inb(ata_primary.io_base + ATA_REG_STATUS);

    if (ata_primary.status & ATA_SR_ERR)
    {
        ata_primary.error = inb(ata_primary.io_base + ATA_REG_ERROR);
        serial_print("ATA Primary Error: ");
        serial_print_hex(ata_primary.error);
        serial_print("\n");
    }

    // interrupt was received
    ata_primary.irq_invoked = 1;
}

// This gets called from the ASM handler for secondary bus
void ata_secondary_handle_interrupt(void)
{
    // Read status to acknowledge interrupt
    ata_secondary.status = inb(ata_secondary.io_base + ATA_REG_STATUS);

    // Check for errors
    if (ata_secondary.status & ATA_SR_ERR)
    {
        ata_secondary.error = inb(ata_secondary.io_base + ATA_REG_ERROR);
        serial_print("ATA Secondary Error: ");
        serial_print_hex(ata_secondary.error);
        serial_print("\n");
    }

    // Set flag to indicate interrupt was received
    ata_secondary.irq_invoked = 1;
}

void ata_init(void)
{
    serial_print("Initializing ATA controller...\n");

    // Initialize primary bus
    ata_primary.io_base = ATA_PRIMARY_IO;
    ata_primary.control_base = ATA_PRIMARY_CONTROL;
    ata_primary.irq_number = ATA_PRIMARY_IRQ;
    ata_primary.irq_invoked = 0;

    // Initialize secondary bus
    ata_secondary.io_base = ATA_SECONDARY_IO;
    ata_secondary.control_base = ATA_SECONDARY_CONTROL;
    ata_secondary.irq_number = ATA_SECONDARY_IRQ;
    ata_secondary.irq_invoked = 0;

    // Disable interrupts during setup
    outb(ata_primary.control_base + ATA_REG_CONTROL, 0x02);
    outb(ata_secondary.control_base + ATA_REG_CONTROL, 0x02);

    // Software reset for primary bus
    outb(ata_primary.control_base + ATA_REG_CONTROL, 0x04);
    for (volatile int i = 0; i < 1000; i++)
        ; // delay
    outb(ata_primary.control_base + ATA_REG_CONTROL, 0x00);

    // Wait for primary to be ready
    ata_wait_ready(&ata_primary);

    // Software reset for secondary bus
    outb(ata_secondary.control_base + ATA_REG_CONTROL, 0x04);
    for (volatile int i = 0; i < 1000; i++)
        ; // delay
    outb(ata_secondary.control_base + ATA_REG_CONTROL, 0x00);

    // Wait for secondary to be ready
    ata_wait_ready(&ata_secondary);

    // Register handlers
    serial_print("Registering ATA interrupt handlers\n");
    idt_set_gate(0x2E, (uint64_t)ata_primary_interrupt_handler);   // IRQ 14 (Primary)
    idt_set_gate(0x2F, (uint64_t)ata_secondary_interrupt_handler); // IRQ 15 (Secondary)

    // Unmask IRQ14 and IRQ15 (both are on the slave PIC)
    // IRQ14 = bit 6 on slave, IRQ15 = bit 7 on slave
    uint8_t master_mask = inb(0x21);
    master_mask &= ~(1 << 2); // unmask cascade to slave (IRQ2)
    outb(0x21, master_mask);

    uint8_t follower_mask = inb(0xA1);
    follower_mask &= ~(1 << 6); // IRQ14 (primary ATA)
    follower_mask &= ~(1 << 7); // IRQ15 (secondary ATA)
    outb(0xA1, follower_mask);

    serial_print("ATA controller initialized!\n");
}

int ata_identify_device(uint8_t drive, uint16_t *identify)
{
    ata_device_t *dev = (drive < 2) ? &ata_primary : &ata_secondary;
    uint16_t io = dev->io_base;

    /* Select drive (master/slave) */
    outb(io + ATA_REG_DRIVE, 0xA0 | ((drive & 1) << 4));

    ata_io_wait(dev);

    /* Zero registers as required by ATA spec */
    outb(io + ATA_SR_IDX, 0);
    outb(io + ATA_REG_LBA_LOW, 0);
    outb(io + ATA_REG_LBA_MID, 0);
    outb(io + ATA_REG_LBA_HIGH, 0);

    /* Send IDENTIFY command */
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    /* Read status */
    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0)
        return -1; // No device

    /* Wait for BSY clear and DRQ set */
    while (status & ATA_SR_BSY)
        status = inb(io + ATA_REG_STATUS);

    if (status & ATA_SR_ERR)
    {
        serial_print("ATA IDENTIFY error\n");
        return -1;
    }

    while (!(status & ATA_SR_DRQ))
    {
        if (status & ATA_SR_ERR)
            return -1;
        status = inb(io + ATA_REG_STATUS);
    }

    /* Read IDENTIFY data (256 words) */
    for (int i = 0; i < 256; i++)
        identify[i] = inw(io + ATA_REG_DATA);

    return 0;
}

// wait for interrupt w/o timout
int ata_wait_irq(ata_device_t *dev)
{
    uint32_t timeout = 1000000;

    while (timeout--)
    {
        if (dev->irq_invoked)
        {
            dev->irq_invoked = 0;
            return 0; // Success
        }
    }

    serial_print("ATA IRQ timeout\n");
    return -1; // Timeout
}

int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint8_t *buffer)
{
    ata_device_t *dev = (drive < 2) ? &ata_primary : &ata_secondary;
    uint16_t io = dev->io_base;

    // Check initial status
    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0xFF)
    {
        // Floating bus - no drive present
        return -1;
    }

    if (ata_wait_not_busy(dev) != 0)
        return -1;

    outb(io + ATA_REG_DRIVE, 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    ata_io_wait(dev); // Give drive time to respond to drive select
    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(io + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(io + ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (uint8_t s = 0; s < count; s++)
    {
        if (ata_wait_drq(dev) != 0)
            return -1;

        uint16_t *buf16 = (uint16_t *)(buffer + s * 512);
        for (int i = 0; i < 256; i++)
            buf16[i] = inw(io + ATA_REG_DATA);
    }

    return 0;
}

int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint8_t *buffer)
{
    ata_device_t *dev = (drive < 2) ? &ata_primary : &ata_secondary;
    uint16_t io = dev->io_base;

    if (ata_wait_not_busy(dev) != 0)
        return -1;

    outb(io + ATA_REG_DRIVE, 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    ata_io_wait(dev);
    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(io + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(io + ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (uint8_t s = 0; s < count; s++)
    {
        ata_io_wait(dev);
        if (ata_wait_drq(dev) != 0)
            return -1;

        uint16_t *buf16 = (uint16_t *)(buffer + s * 512);
        for (int i = 0; i < 256; i++)
            outw(io + ATA_REG_DATA, buf16[i]);

        ata_io_wait(dev);
        /* Wait for drive to finish processing this sector */
        if (ata_wait_not_busy(dev) != 0)
            return -1;
    }

    /* Flush write cache */
    outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_io_wait(dev);
    if (ata_wait_not_busy(dev) != 0)
        return -1;

    return 0;
}

int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t *buffer)
{
    return ata_read_sectors(drive, lba, 1, buffer);
}

int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t *buffer)
{
    return ata_write_sectors(drive, lba, 1, buffer);
}

uint8_t ata_get_status(uint8_t drive)
{
    ata_device_t *dev = (drive < 2) ? &ata_primary : &ata_secondary;
    return dev->status;
}

uint8_t ata_get_error(uint8_t drive)
{
    ata_device_t *dev = (drive < 2) ? &ata_primary : &ata_secondary;
    return dev->error;
}