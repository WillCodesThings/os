#pragma once
#include <stdint.h>

// ATA device structure
typedef struct {
    volatile uint8_t irq_invoked;
    volatile uint8_t status;
    volatile uint8_t error;
    uint16_t io_base;
    uint16_t control_base;
    uint8_t irq_number;
} ata_device_t;

// Drive selection constants
#define ATA_PRIMARY_MASTER 0
#define ATA_PRIMARY_FOLLOWER 1
#define ATA_SECONDARY_MASTER 2
#define ATA_SECONDARY_FOLLOWER 3

// ATA Primary Bus I/O Ports
#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_PRIMARY_IRQ 14

// ATA Secondary Bus I/O Ports
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_CONTROL 0x376
#define ATA_SECONDARY_IRQ 15

// ATA Register Offsets
#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA_LOW 3
#define ATA_REG_LBA_MID 4
#define ATA_REG_LBA_HIGH 5
#define ATA_REG_DRIVE 6
#define ATA_REG_STATUS 7
#define ATA_REG_COMMAND 7

// ATA Control Registers
#define ATA_REG_CONTROL 0
#define ATA_REG_ALTSTATUS 0

// Status Register Bits
#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DSC 0x10
#define ATA_SR_DRQ 0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX 0x02
#define ATA_SR_ERR 0x01

// ATA Commands
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

// ATA drive types
#define ATA_DRIVE_MASTER 0xE0
#define ATA_DRIVE_SLAVE  0xF0

// ATA functions
void ata_init(void);
int ata_identify_device(uint8_t drive, uint16_t *identify);

int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t *buffer);
int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t *buffer);

int ata_read_sector(uint8_t drive, uint32_t lba, uint16_t *buffer);
int ata_write_sector(uint8_t drive, uint32_t lba, uint16_t *buffer);

uint8_t ata_get_status(uint8_t drive);
uint8_t ata_get_error(uint8_t drive);
