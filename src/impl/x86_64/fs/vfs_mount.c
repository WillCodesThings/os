#include <fs/vfs_mount.h>
#include <fs/vfs.h>
#include <fs/simplefs.h>
#include <disk/partition.h>
#include <disk/partition_block_device.h>
#include <memory/heap.h>
#include <utils/log.h>
#include <drivers/serial.h>
#include <utils/string.h>

typedef struct
{
    char mount_path[256];
    vfs_node_t *mount_node;
    block_device_t *block_device;
    partition_info_t *partition;
    char fs_type[32];
    int is_active;
} mount_entry_t;

#define MAX_MOUNTS 16
static mount_entry_t mount_table[MAX_MOUNTS];
static int mount_count = 0;

void vfs_mount_init(void)
{
    LOG_INFO("VFS", "Initializing VFS mount system...");
    mount_count = 0;

    for (int i = 0; i < MAX_MOUNTS; i++)
    {
        mount_table[i].mount_path[0] = '\0';
        mount_table[i].mount_node = NULL;
        mount_table[i].block_device = NULL;
        mount_table[i].partition = NULL;
        mount_table[i].fs_type[0] = '\0';
        mount_table[i].is_active = 0;
    }
}

int vfs_mount_partition(const char *mount_path, uint8_t drive, uint8_t partition_index, const char *fs_type)
{
    if (mount_count >= MAX_MOUNTS)
    {
        LOG_ERROR("VFS", "Mount table full");
        return -1;
    }

    LOG_INFO("VFS", "Mounting %s from drive %x, partition %x", mount_path, (uint32_t)drive, (uint32_t)partition_index);

    partition_info_t *partition = partition_get(drive, partition_index);
    if (!partition)
    {
        LOG_ERROR("VFS", "Partition not found");
        return -1;
    }

    block_device_t *part_block_dev = partition_create_block_device(partition);
    if (!part_block_dev)
    {
        LOG_ERROR("VFS", "Failed to create partition block device");
        return -1;
    }

    vfs_node_t *mount_node = NULL;

    if (strcmp(fs_type, "simplefs") == 0)
    {
        simplefs_init(part_block_dev);

        if (simplefs_mount(part_block_dev) != 0)
        {
            LOG_WARN("VFS", "No valid filesystem, formatting...");

            uint32_t total_blocks = partition->num_sectors;
            simplefs_format(part_block_dev, total_blocks, 20);

            if (simplefs_mount(part_block_dev) != 0)
            {
                LOG_ERROR("VFS", "Failed to mount after format");
                return -1;
            }
        }

        mount_node = vfs_get_root();
    }
    else
    {
        LOG_ERROR("VFS", "Unsupported filesystem type: %s", fs_type);
        return -1;
    }

    mount_entry_t *entry = &mount_table[mount_count];
    strcpy(entry->mount_path, mount_path);
    entry->mount_node = mount_node;
    entry->block_device = part_block_dev;
    entry->partition = partition;
    strcpy(entry->fs_type, fs_type);
    entry->is_active = 1;
    mount_count++;

    LOG_INFO("VFS", "Successfully mounted %s", mount_path);

    return 0;
}

int vfs_unmount(const char *mount_path)
{
    for (int i = 0; i < mount_count; i++)
    {
        if (strcmp(mount_table[i].mount_path, mount_path) == 0 && mount_table[i].is_active)
        {
            mount_table[i].is_active = 0;
            LOG_INFO("VFS", "Unmounted %s", mount_path);
            return 0;
        }
    }

    LOG_WARN("VFS", "Mount point not found: %s", mount_path);
    return -1;
}

partition_info_t *vfs_get_partition_for_path(const char *path)
{
    int best_match_len = 0;
    partition_info_t *best_partition = NULL;

    for (int i = 0; i < mount_count; i++)
    {
        if (!mount_table[i].is_active)
            continue;

        int mount_len = strlen(mount_table[i].mount_path);

        if (strncmp(path, mount_table[i].mount_path, mount_len) == 0)
        {
            if (mount_len > best_match_len)
            {
                best_match_len = mount_len;
                best_partition = mount_table[i].partition;
            }
        }
    }

    return best_partition;
}

block_device_t *vfs_get_block_device_for_path(const char *path)
{
    int best_match_len = 0;
    block_device_t *best_device = NULL;

    for (int i = 0; i < mount_count; i++)
    {
        if (!mount_table[i].is_active)
            continue;

        int mount_len = strlen(mount_table[i].mount_path);

        if (strncmp(path, mount_table[i].mount_path, mount_len) == 0)
        {
            if (mount_len > best_match_len)
            {
                best_match_len = mount_len;
                best_device = mount_table[i].block_device;
            }
        }
    }

    return best_device;
}

void vfs_list_mounts(void)
{
    LOG_INFO("VFS", "=== Mounted Filesystems ===");

    int active_count = 0;
    for (int i = 0; i < mount_count; i++)
    {
        if (!mount_table[i].is_active)
            continue;

        active_count++;
        mount_entry_t *entry = &mount_table[i];

        LOG_INFO("VFS", "%s -> Drive %x, Partition %x (%s)",
                 entry->mount_path,
                 (uint32_t)entry->partition->drive,
                 (uint32_t)entry->partition->partition_index,
                 entry->fs_type);

        LOG_INFO("VFS", "  Start LBA: %x, Size: %x sectors",
                 entry->partition->lba_start,
                 entry->partition->num_sectors);
    }

    if (active_count == 0)
    {
        LOG_INFO("VFS", "No filesystems mounted");
    }
}