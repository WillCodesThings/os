#include <interrupts/safeInterrupt.h>
#include <interrupts/io/keyboard.h>
#include <interrupts/io/mouse.h>
#include <interrupts/io/ata.h>

#include <graphics/graphics.h>
#include <graphics/cursor.h>

#include <shell/print.h>

#include <disk/partition.h>
#include <disk/partition_block_device.h>
#include <disk/test.h>

#include <fs/vfs.h>
#include <fs/vfs_mount.h>
#include <fs/simplefs.h>
#include <fs/tmpfs.h>

#include <memory/heap.h>
#include <utils/log.h>

void kernel_filesystem_init(void)
{
    LOG_INFO("FS", "=== Filesystem Initialization ===");

    LOG_INFO("FS", "[1/7] Scanning for partitions...");
    partition_init();

    LOG_INFO("FS", "[2/7] Initializing ATA block devices...");

    partition_info_t *p = partition_get(ATA_PRIMARY_MASTER, 0);
    if (!p)
    {
        LOG_WARN("FS", "No partitions found on primary master");
        if (partition_auto_create(ATA_PRIMARY_MASTER) != 0)
        {
            LOG_WARN("FS", "No disk available - falling back to tmpfs");
            tmpfs_init();
            LOG_INFO("FS", "=== Filesystem Initialization Complete (tmpfs) ===");
            return;
        }
        LOG_INFO("FS", "Auto MBR partitioning successful");
        LOG_INFO("FS", "Re-initializing partitions...");
        partition_init();
        p = partition_get(ATA_PRIMARY_MASTER, 0);
        if (!p)
        {
            LOG_WARN("FS", "Failed to re-initialize partitions - falling back to tmpfs");
            tmpfs_init();
            LOG_INFO("FS", "=== Filesystem Initialization Complete (tmpfs) ===");
            return;
        }
    }

    // Wrap the partition as its own block device
    block_device_t *part = partition_create_block_device(p);
    (void)part;  // Suppress unused warning

    LOG_INFO("FS", "[3/7] Detected partitions:");
    partition_list();

    LOG_INFO("FS", "[4/7] Initializing VFS...");
    vfs_init();

    LOG_INFO("FS", "[5/7] Initializing mount system...");
    vfs_mount_init();

    LOG_INFO("FS", "[6/7] Mounting root filesystem...");
    if (vfs_mount_partition("/", ATA_PRIMARY_MASTER, 0, "simplefs") == 0)
    {
        LOG_INFO("FS", "Root filesystem mounted successfully");
    }
    else
    {
        LOG_ERROR("FS", "Failed to mount simplefs - falling back to tmpfs");
        tmpfs_init();
    }

    LOG_INFO("FS", "[7/7] Current mount configuration:");
    vfs_list_mounts();

    LOG_INFO("FS", "=== Filesystem Initialization Complete ===");
}