#pragma once
#include <stdint.h>

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_FEATURES 0x1F1
#define ATA_SECTOR_COUNT0 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRIVE_SELECT 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30