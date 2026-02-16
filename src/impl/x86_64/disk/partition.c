#include <disk/partition.h>
#include <interrupts/io/ata.h>
#include <memory/heap.h>
#include <utils/memory.h>
#include <utils/log.h>
#include <drivers/serial.h>

// MBR Partition Table Entry
typedef struct
{
    uint8_t status;         // 0x80 = bootable, 0x00 = non-bootable
    uint8_t first_chs[3];   // CHS address of first sector (legacy)
    uint8_t partition_type; // Partition type
    uint8_t last_chs[3];    // CHS address of last sector (legacy)
    uint32_t lba_start;     // LBA of first sector
    uint32_t num_sectors;   // Number of sectors in partition
} __attribute__((packed)) partition_entry_t;

// MBR Structure
typedef struct
{
    uint8_t bootloader[446];
    partition_entry_t partitions[4];
    uint16_t signature; // 0xAA55
} __attribute__((packed)) mbr_t;

// Store detected partitions
#define MAX_PARTITIONS 16
static partition_info_t partitions[MAX_PARTITIONS];
static int partition_count = 0;

// Read and parse MBR from a drive
static int read_partition_table(uint8_t drive)
{
    uint8_t mbr_buffer[512];

    LOG_DEBUG("DISK", "Reading partition table from drive %x", (uint32_t)drive);

    // Read MBR (sector 0)
    if (ata_read_sector(drive, 0, mbr_buffer) != 0)
    {
        LOG_ERROR("DISK", "Failed to read MBR");
        return -1;
    }

    mbr_t *mbr = (mbr_t *)mbr_buffer;

    // Check MBR signature
    if (mbr->signature != 0xAA55)
    {
        LOG_WARN("DISK", "Invalid MBR signature: %x", (uint32_t)mbr->signature);
        return -1;
    }

    LOG_DEBUG("DISK", "Valid MBR found");

    // Parse partition entries
    for (int i = 0; i < 4; i++)
    {
        partition_entry_t *entry = &mbr->partitions[i];

        if (entry->partition_type == PART_TYPE_EMPTY)
            continue;

        if (partition_count >= MAX_PARTITIONS)
        {
            LOG_WARN("DISK", "Too many partitions detected");
            break;
        }

        partition_info_t *part = &partitions[partition_count++];
        part->drive = drive;
        part->partition_index = i;
        part->lba_start = entry->lba_start;
        part->num_sectors = entry->num_sectors;
        part->type = entry->partition_type;
        part->bootable = (entry->status == 0x80);

        LOG_INFO("DISK", "Partition %x: Type=%x Start=%x Size=%x sectors",
                 (uint32_t)i,
                 (uint32_t)entry->partition_type,
                 entry->lba_start,
                 entry->num_sectors);
    }

    return 0;
}

void partition_init(void)
{
    LOG_INFO("DISK", "Scanning for partitions...");

    partition_count = 0;

    // Scan all possible drives
    for (uint8_t drive = 0; drive <= ATA_SECONDARY_FOLLOWER; drive++)
    {
        read_partition_table(drive);
    }

    LOG_INFO("DISK", "Found %x partitions", (uint32_t)partition_count);
}

partition_info_t *partition_get(uint8_t drive, uint8_t partition_index)
{
    for (int i = 0; i < partition_count; i++)
    {
        if (partitions[i].drive == drive &&
            partitions[i].partition_index == partition_index)
        {
            return &partitions[i];
        }
    }
    return NULL;
}

void partition_list(void)
{
    LOG_INFO("DISK", "=== Detected Partitions ===");

    for (int i = 0; i < partition_count; i++)
    {
        partition_info_t *p = &partitions[i];

        const char *type_name;
        switch (p->type)
        {
        case PART_TYPE_FAT16:
            type_name = "FAT16";
            break;
        case PART_TYPE_FAT32:
        case PART_TYPE_FAT32_LBA:
            type_name = "FAT32";
            break;
        case PART_TYPE_NTFS:
            type_name = "NTFS";
            break;
        case PART_TYPE_LINUX:
            type_name = "Linux";
            break;
        case PART_TYPE_LINUX_SWAP:
            type_name = "Linux Swap";
            break;
        default:
            type_name = NULL;
        }

        if (type_name)
        {
            LOG_INFO("DISK", "Drive %x, Partition %x: %s, Start LBA: %x, Sectors: %x%s",
                     (uint32_t)p->drive,
                     (uint32_t)p->partition_index,
                     type_name,
                     p->lba_start,
                     p->num_sectors,
                     p->bootable ? " [BOOTABLE]" : "");
        }
        else
        {
            LOG_INFO("DISK", "Drive %x, Partition %x: Type %x, Start LBA: %x, Sectors: %x%s",
                     (uint32_t)p->drive,
                     (uint32_t)p->partition_index,
                     (uint32_t)p->type,
                     p->lba_start,
                     p->num_sectors,
                     p->bootable ? " [BOOTABLE]" : "");
        }
    }
}

int partition_read(partition_info_t *partition, uint32_t sector, uint8_t *buffer)
{
    if (!partition)
        return -1;

    uint32_t absolute_lba = partition->lba_start + sector;

    if (sector >= partition->num_sectors)
    {
        LOG_WARN("DISK", "Read beyond partition bounds");
        return -1;
    }

    int result = ata_read_sector(partition->drive, absolute_lba, buffer);
    return result;
}

int partition_write(partition_info_t *partition, uint32_t sector, uint8_t *buffer)
{
    if (!partition)
        return -1;

    uint32_t absolute_lba = partition->lba_start + sector;

    if (sector >= partition->num_sectors)
    {
        LOG_WARN("DISK", "Write beyond partition bounds");
        return -1;
    }

    return ata_write_sector(partition->drive, absolute_lba, buffer);
}

// Create MBR with partitions on an empty drive
int partition_create_mbr(uint8_t drive, uint32_t total_sectors)
{
    LOG_INFO("DISK", "Creating MBR partition table on drive %x", (uint32_t)drive);

    uint8_t mbr_buffer[512];
    memset(mbr_buffer, 0, 512);

    mbr_t *mbr = (mbr_t *)mbr_buffer;

    // Calculate partition sizes (split disk in half)
    uint32_t partition1_start = 2048; // Start at 1MB (standard alignment)
    uint32_t partition1_size = (total_sectors - 2048) / 2;
    uint32_t partition2_start = partition1_start + partition1_size;
    uint32_t partition2_size = total_sectors - partition2_start;

    // Partition 1 (bootable)
    mbr->partitions[0].status = 0x80; // Bootable
    mbr->partitions[0].partition_type = PART_TYPE_LINUX;
    mbr->partitions[0].lba_start = partition1_start;
    mbr->partitions[0].num_sectors = partition1_size;

    // Set CHS values (legacy, but some BIOSes check them)
    mbr->partitions[0].first_chs[0] = 0x00;
    mbr->partitions[0].first_chs[1] = 0x02;
    mbr->partitions[0].first_chs[2] = 0x00;
    mbr->partitions[0].last_chs[0] = 0xFF;
    mbr->partitions[0].last_chs[1] = 0xFF;
    mbr->partitions[0].last_chs[2] = 0xFF;

    // Partition 2
    mbr->partitions[1].status = 0x00; // Not bootable
    mbr->partitions[1].partition_type = PART_TYPE_LINUX;
    mbr->partitions[1].lba_start = partition2_start;
    mbr->partitions[1].num_sectors = partition2_size;

    mbr->partitions[1].first_chs[0] = 0xFF;
    mbr->partitions[1].first_chs[1] = 0xFF;
    mbr->partitions[1].first_chs[2] = 0xFF;
    mbr->partitions[1].last_chs[0] = 0xFF;
    mbr->partitions[1].last_chs[1] = 0xFF;
    mbr->partitions[1].last_chs[2] = 0xFF;

    // Partitions 3 and 4 are empty (already zeroed by memset)

    // Set MBR signature
    mbr->signature = 0xAA55;

    // Write MBR to sector 0
    LOG_INFO("DISK", "Writing MBR to disk...");
    if (ata_write_sector(drive, 0, mbr_buffer) != 0)
    {
        LOG_ERROR("DISK", "Failed to write MBR");
        return -1;
    }

    LOG_INFO("DISK", "MBR created successfully!");
    LOG_INFO("DISK", "Partition 1: Start=%x, Size=%x sectors (bootable)",
             partition1_start, partition1_size);
    LOG_INFO("DISK", "Partition 2: Start=%x, Size=%x sectors",
             partition2_start, partition2_size);

    return 0;
}

// Create custom MBR with specific partition layout
int partition_create_mbr_custom(uint8_t drive,
                                uint32_t part1_start, uint32_t part1_size,
                                uint32_t part2_start, uint32_t part2_size,
                                uint8_t part1_type, uint8_t part2_type)
{
    LOG_INFO("DISK", "Creating custom MBR on drive %x", (uint32_t)drive);

    uint8_t mbr_buffer[512];
    memset(mbr_buffer, 0, 512);

    mbr_t *mbr = (mbr_t *)mbr_buffer;

    // Partition 1
    if (part1_size > 0)
    {
        mbr->partitions[0].status = 0x80; // Bootable
        mbr->partitions[0].partition_type = part1_type;
        mbr->partitions[0].lba_start = part1_start;
        mbr->partitions[0].num_sectors = part1_size;

        mbr->partitions[0].first_chs[0] = 0x00;
        mbr->partitions[0].first_chs[1] = 0x02;
        mbr->partitions[0].first_chs[2] = 0x00;
        mbr->partitions[0].last_chs[0] = 0xFF;
        mbr->partitions[0].last_chs[1] = 0xFF;
        mbr->partitions[0].last_chs[2] = 0xFF;

        LOG_INFO("DISK", "Partition 1: Type=%x, Start=%x, Size=%x sectors",
                 (uint32_t)part1_type, part1_start, part1_size);
    }

    // Partition 2
    if (part2_size > 0)
    {
        mbr->partitions[1].status = 0x00;
        mbr->partitions[1].partition_type = part2_type;
        mbr->partitions[1].lba_start = part2_start;
        mbr->partitions[1].num_sectors = part2_size;

        mbr->partitions[1].first_chs[0] = 0xFF;
        mbr->partitions[1].first_chs[1] = 0xFF;
        mbr->partitions[1].first_chs[2] = 0xFF;
        mbr->partitions[1].last_chs[0] = 0xFF;
        mbr->partitions[1].last_chs[1] = 0xFF;
        mbr->partitions[1].last_chs[2] = 0xFF;

        LOG_INFO("DISK", "Partition 2: Type=%x, Start=%x, Size=%x sectors",
                 (uint32_t)part2_type, part2_start, part2_size);
    }

    // Set MBR signature
    mbr->signature = 0xAA55;

    // Write MBR
    if (ata_write_sector(drive, 0, mbr_buffer) != 0)
    {
        LOG_ERROR("DISK", "Failed to write MBR");
        return -1;
    }

    LOG_INFO("DISK", "Custom MBR created successfully!");
    return 0;
}

// Auto-partition: detects disk size and creates MBR
int partition_auto_create(uint8_t drive)
{
    LOG_INFO("DISK", "Auto-partitioning drive %x", (uint32_t)drive);

    uint16_t identify[256];

    if (ata_identify_device(drive, identify) != 0)
    {
        LOG_ERROR("DISK", "Could not detect disk size (IDENTIFY DEVICE failed)");
        return -1;
    }

    uint64_t total_sectors = 0;

    /* Prefer LBA48 if supported */
    if (identify[83] & (1 << 10))
    {
        total_sectors =
            ((uint64_t)identify[103] << 48) |
            ((uint64_t)identify[102] << 32) |
            ((uint64_t)identify[101] << 16) |
            identify[100];
    }
    else
    {
        total_sectors =
            ((uint32_t)identify[61] << 16) |
            identify[60];
    }

    if (total_sectors == 0)
    {
        LOG_ERROR("DISK", "Detected zero-sized disk");
        return -1;
    }

    LOG_INFO("DISK", "Detected disk size: %x sectors (%x MB)",
             (uint32_t)total_sectors, (uint32_t)(total_sectors / 2048));

    /* MBR limit: 2^32 − 1 sectors */
    if (total_sectors > 0xFFFFFFFFULL)
    {
        LOG_ERROR("DISK", "Disk too large for MBR, GPT required");
        return -1;
    }

    /* Require minimum space for alignment + partitions */
    if (total_sectors < 4096)
    {
        LOG_ERROR("DISK", "Disk too small for partitioning");
        return -1;
    }

    /* Create default MBR layout */
    return partition_create_mbr(drive, (uint32_t)total_sectors);
}
