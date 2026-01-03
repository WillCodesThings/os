#pragma once

#include <stdint.h>

typedef struct {
    uint8_t drive;
    uint8_t partition_index;
    uint32_t lba_start;
    uint32_t num_sectors;
    uint8_t type;
    uint8_t bootable;
} partition_info_t;

void partition_init(void);
partition_info_t *partition_get(uint8_t drive, uint8_t partition_index);

void partition_list(void);

int partition_read(partition_info_t *partition, uint32_t sector, uint16_t *buffer);
int partition_write(partition_info_t *partition, uint32_t sector, uint16_t *buffer);

int partition_create_mbr(uint8_t drive, uint32_t total_sectors);

int partition_create_mbr_custom(uint8_t drive, 
                                 uint32_t part1_start, uint32_t part1_size,
                                 uint32_t part2_start, uint32_t part2_size,
                                 uint8_t part1_type, uint8_t part2_type);

int partition_auto_create(uint8_t drive);

#define PART_TYPE_EMPTY 0x00
#define PART_TYPE_FAT12 0x01
#define PART_TYPE_FAT16_SMALL 0x04
#define PART_TYPE_EXTENDED 0x05
#define PART_TYPE_FAT16 0x06
#define PART_TYPE_NTFS 0x07
#define PART_TYPE_FAT32 0x0B
#define PART_TYPE_FAT32_LBA 0x0C
#define PART_TYPE_FAT16_LBA 0x0E
#define PART_TYPE_LINUX 0x83
#define PART_TYPE_LINUX_SWAP 0x82
#define PART_TYPE_LINUX_LVM 0x8E
