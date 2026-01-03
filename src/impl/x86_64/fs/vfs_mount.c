#include <fs/vfs_mount.h>
#include <fs/vfs.h>
#include <fs/simplefs.h>
#include <disk/partition.h>
#include <disk/partition_block_device.h>
#include <memory/heap.h>
#include <shell/print.h>
#include <utils/string.h>

typedef struct {
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
    serial_print("Initializing VFS mount system...\n");
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
        serial_print("Mount table full\n");
        return -1;
    }
    
    serial_print("Mounting ");
    serial_print(mount_path);
    serial_print(" from drive ");
    serial_print_hex(drive);
    serial_print(", partition ");
    serial_print_hex(partition_index);
    serial_print("\n");
    
    partition_info_t *partition = partition_get(drive, partition_index);
    if (!partition)
    {
        serial_print("Partition not found\n");
        return -1;
    }
    
    block_device_t *part_block_dev = partition_create_block_device(partition);
    if (!part_block_dev)
    {
        serial_print("Failed to create partition block device\n");
        return -1;
    }
    
    vfs_node_t *mount_node = NULL;
    
    if (strcmp(fs_type, "simplefs") == 0)
    {
        simplefs_init(part_block_dev);
        
        if (simplefs_mount(part_block_dev) != 0)
        {
            serial_print("No valid filesystem found, formatting...\n");
            
            uint32_t total_blocks = partition->num_sectors / 8;
            simplefs_format(part_block_dev, total_blocks, 20);
            
            if (simplefs_mount(part_block_dev) != 0)
            {
                serial_print("Failed to mount after format\n");
                return -1;
            }
        }
        
        mount_node = vfs_get_root();
    }
    else
    {
        serial_print("Unsupported filesystem type: ");
        serial_print(fs_type);
        serial_print("\n");
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
    
    serial_print("Successfully mounted ");
    serial_print(mount_path);
    serial_print("\n");
    
    return 0;
}

int vfs_unmount(const char *mount_path)
{
    for (int i = 0; i < mount_count; i++)
    {
        if (strcmp(mount_table[i].mount_path, mount_path) == 0 && mount_table[i].is_active)
        {
            mount_table[i].is_active = 0;
            serial_print("Unmounted ");
            serial_print(mount_path);
            serial_print("\n");
            return 0;
        }
    }
    
    serial_print("Mount point not found: ");
    serial_print(mount_path);
    serial_print("\n");
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
    serial_print("=== Mounted Filesystems ===\n");
    
    int active_count = 0;
    for (int i = 0; i < mount_count; i++)
    {
        if (!mount_table[i].is_active)
            continue;
        
        active_count++;
        mount_entry_t *entry = &mount_table[i];
        
        serial_print(entry->mount_path);
        serial_print(" -> Drive ");
        serial_print_hex(entry->partition->drive);
        serial_print(", Partition ");
        serial_print_hex(entry->partition->partition_index);
        serial_print(" (");
        serial_print(entry->fs_type);
        serial_print(")\n");
        
        serial_print("  Start LBA: ");
        serial_print_hex(entry->partition->lba_start);
        serial_print(", Size: ");
        serial_print_hex(entry->partition->num_sectors);
        serial_print(" sectors\n");
    }
    
    if (active_count == 0)
    {
        serial_print("No filesystems mounted\n");
    }
}