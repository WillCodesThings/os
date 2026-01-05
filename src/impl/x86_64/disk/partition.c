#include <disk/partition.h>
#include <interrupts/io/ata.h>
#include <memory/heap.h>
#include <utils/memory.h>
#include <shell/shell.h>
#include <shell/print.h>

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

    serial_print("Reading partition table from drive ");
    serial_print_hex(drive);
    serial_print("\n");

    // Read MBR (sector 0)
    if (ata_read_sector(drive, 0, mbr_buffer) != 0)
    {
        serial_print("Failed to read MBR\n");
        return -1;
    }

    mbr_t *mbr = (mbr_t *)mbr_buffer;

    // Check MBR signature
    if (mbr->signature != 0xAA55)
    {
        serial_print("Invalid MBR signature: ");
        serial_print_hex(mbr->signature);
        serial_print("\n");
        return -1;
    }

    serial_print("Valid MBR found!\n");

    // Parse partition entries
    for (int i = 0; i < 4; i++)
    {
        partition_entry_t *entry = &mbr->partitions[i];

        if (entry->partition_type == PART_TYPE_EMPTY)
            continue;

        if (partition_count >= MAX_PARTITIONS)
        {
            serial_print("Too many partitions detected\n");
            break;
        }

        partition_info_t *part = &partitions[partition_count++];
        part->drive = drive;
        part->partition_index = i;
        part->lba_start = entry->lba_start;
        part->num_sectors = entry->num_sectors;
        part->type = entry->partition_type;
        part->bootable = (entry->status == 0x80);

        serial_print("Partition ");
        serial_print_hex(i);
        serial_print(": Type=");
        serial_print_hex(entry->partition_type);
        serial_print(" Start=");
        serial_print_hex(entry->lba_start);
        serial_print(" Size=");
        serial_print_hex(entry->num_sectors);
        serial_print(" sectors\n");
    }

    return 0;
}

void partition_init(void)
{
    serial_print("Scanning for partitions...\n");

    partition_count = 0;

    // Scan all possible drives
    for (uint8_t drive = 0; drive <= ATA_SECONDARY_FOLLOWER; drive++)
    {
        read_partition_table(drive);
    }

    serial_print("Found ");
    serial_print_hex(partition_count);
    serial_print(" partitions\n");
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
    serial_print("=== Detected Partitions ===\n");

    for (int i = 0; i < partition_count; i++)
    {
        partition_info_t *p = &partitions[i];

        serial_print("Drive ");
        serial_print_hex(p->drive);
        serial_print(", Partition ");
        serial_print_hex(p->partition_index);
        serial_print(": ");

        switch (p->type)
        {
        case PART_TYPE_FAT16:
            serial_print("FAT16");
            break;
        case PART_TYPE_FAT32:
        case PART_TYPE_FAT32_LBA:
            serial_print("FAT32");
            break;
        case PART_TYPE_NTFS:
            serial_print("NTFS");
            break;
        case PART_TYPE_LINUX:
            serial_print("Linux");
            break;
        case PART_TYPE_LINUX_SWAP:
            serial_print("Linux Swap");
            break;
        default:
            serial_print("Type ");
            serial_print_hex(p->type);
        }

        serial_print(", Start LBA: ");
        serial_print_hex(p->lba_start);
        serial_print(", Sectors: ");
        serial_print_hex(p->num_sectors);

        if (p->bootable)
            serial_print(" [BOOTABLE]");

        serial_print("\n");
    }
}

int partition_read(partition_info_t *partition, uint32_t sector, uint8_t *buffer)
{
    if (!partition)
        return -1;

    uint32_t absolute_lba = partition->lba_start + sector;

    if (sector >= partition->num_sectors)
    {
        serial_print("Read beyond partition bounds\n");
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
        serial_print("Write beyond partition bounds\n");
        return -1;
    }

    return ata_write_sector(partition->drive, absolute_lba, buffer);
}

// Create MBR with partitions on an empty drive
int partition_create_mbr(uint8_t drive, uint32_t total_sectors)
{
    serial_print("Creating MBR partition table on drive ");
    serial_print_hex(drive);
    serial_print("\n");

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
    serial_print("Writing MBR to disk...\n");
    if (ata_write_sector(drive, 0, mbr_buffer) != 0)
    {
        serial_print("Failed to write MBR\n");
        return -1;
    }

    serial_print("MBR created successfully!\n");
    serial_print("Partition 1: Start=");
    serial_print_hex(partition1_start);
    serial_print(", Size=");
    serial_print_hex(partition1_size);
    serial_print(" sectors (bootable)\n");

    serial_print("Partition 2: Start=");
    serial_print_hex(partition2_start);
    serial_print(", Size=");
    serial_print_hex(partition2_size);
    serial_print(" sectors\n");

    return 0;
}

// Create custom MBR with specific partition layout
int partition_create_mbr_custom(uint8_t drive,
                                uint32_t part1_start, uint32_t part1_size,
                                uint32_t part2_start, uint32_t part2_size,
                                uint8_t part1_type, uint8_t part2_type)
{
    serial_print("Creating custom MBR on drive ");
    serial_print_hex(drive);
    serial_print("\n");

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

        serial_print("Partition 1: Type=");
        serial_print_hex(part1_type);
        serial_print(", Start=");
        serial_print_hex(part1_start);
        serial_print(", Size=");
        serial_print_hex(part1_size);
        serial_print(" sectors\n");
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

        serial_print("Partition 2: Type=");
        serial_print_hex(part2_type);
        serial_print(", Start=");
        serial_print_hex(part2_start);
        serial_print(", Size=");
        serial_print_hex(part2_size);
        serial_print(" sectors\n");
    }

    // Set MBR signature
    mbr->signature = 0xAA55;

    // Write MBR
    if (ata_write_sector(drive, 0, mbr_buffer) != 0)
    {
        serial_print("Failed to write MBR\n");
        return -1;
    }

    serial_print("Custom MBR created successfully!\n");
    return 0;
}

// Auto-partition: detects disk size and creates MBR
int partition_auto_create(uint8_t drive)
{
    serial_print("Auto-partitioning drive ");
    serial_print_hex(drive);
    serial_print("\n");

    uint16_t identify[256];

    if (ata_identify_device(drive, identify) != 0)
    {
        serial_print("Could not detect disk size (IDENTIFY DEVICE failed)\n");
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
        serial_print("Detected zero-sized disk\n");
        return -1;
    }

    serial_print("Detected disk size: ");
    serial_print_hex((uint32_t)total_sectors);
    serial_print(" sectors (");
    serial_print_hex((uint32_t)(total_sectors / 2048));
    serial_print(" MB)\n");

    /* MBR limit: 2^32 − 1 sectors */
    if (total_sectors > 0xFFFFFFFFULL)
    {
        serial_print("Disk too large for MBR, GPT required\n");
        return -1;
    }

    /* Require minimum space for alignment + partitions */
    if (total_sectors < 4096)
    {
        serial_print("Disk too small for partitioning\n");
        return -1;
    }

    /* Create default MBR layout */
    return partition_create_mbr(drive, (uint32_t)total_sectors);
}
