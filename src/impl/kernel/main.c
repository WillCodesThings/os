

#include <interrupts/safeInterrupt.h>
#include <interrupts/io/keyboard.h>
#include <interrupts/io/mouse.h>
#include <interrupts/io/ata.h>

#include <graphics/graphics.h>
#include <graphics/cursor.h>

#include <shell/print.h>
#include <shell/shell.h>

#include <disk/partition.h>
#include <disk/ata_block_device.h>
#include <disk/test.h>

#include <fs/vfs.h>
#include <fs/vfs_mount.h>
#include <fs/simplefs.h>

void kernel_main(void)
{
    // Initialize graphics
    graphics_init();
    print_init();

    init_interrupts_safe();
    // test_interrupts();

    keyboard_init();
    mouse_init();
    ata_init();

    cursor_init();
    cursor_show();

    enable_interrupts();

    kernel_filesystem_init();

    // Draw a welcome screen
    uint32_t cx = get_screen_width() / 2;
    uint32_t cy = get_screen_height() / 2;

    draw_triangle(cx, cy - 100, cx - 100, cy + 100, cx + 100, cy + 100, COLOR_CYAN, 1);
    // draw_triangle_3colors(cx, cy - 80, cx - 80, cy + 80, cx + 80, cy + 80, COLOR_MAGENTA, COLOR_YELLOW, COLOR_GREEN);
    fill_rect(10, 10, 200, 50, COLOR_BLUE);

    kernel_test_filesystem();

    // mouse_test_polling();

    shell_run();
}

void kernel_filesystem_init(void)
{
    serial_print("\n=== Filesystem Initialization ===\n");
    
    serial_print("\n[1/7] Scanning for partitions...\n");
    
    // if (partition_auto_create(ATA_SECONDARY_MASTER) != 0)
    // {
    //     serial_print("Failed to auto-partition secondary master!\n");
    //     return;
    // }
    partition_init();

    serial_print("\n[2/7] Initializing ATA block devices...\n");
    
    block_device_t *disk = ata_create_block_device(ATA_PRIMARY_MASTER);
    partition_info_t *p = partition_get(ATA_PRIMARY_MASTER, 0);
    if (!p) {
        serial_print("No partitions found on primary master!\n");
        if (partition_auto_create(ATA_PRIMARY_MASTER) != 0)
        {
            serial_print("Failed to auto-partition primary master!\n");
            return;
        }
        serial_print("Auto MBR Partitioning successful!\n");
        serial_print("Attempting to re-initialize partitions...\n");
        partition_init();
        p = partition_get(ATA_PRIMARY_MASTER, 0);
        if (!p) {
            serial_print("Failed to re-initialize partitions!\n");
            return;
        }
    }
    

    // Wrap the partition as its own block device
    block_device_t *part = partition_create_block_device(p);
    
    serial_print("\n[3/7] Detected partitions:\n");
    partition_list();
    
    serial_print("\n[4/7] Initializing VFS...\n");
    vfs_init();
    
    serial_print("\n[5/7] Initializing mount system...\n");
    vfs_mount_init();
    
    serial_print("\n[6/7] Mounting root filesystem...\n");
    if (vfs_mount_partition("/", ATA_PRIMARY_MASTER, 0, "simplefs") == 0)
    {
        serial_print("SUCCESS: Root filesystem mounted!\n");
    }
    else
    {
        serial_print("ERROR: Failed to mount root filesystem!\n");
        return;
    }
    
    serial_print("\n[7/7] Current mount configuration:\n");
    vfs_list_mounts();
    
    serial_print("\n=== Filesystem Initialization Complete ===\n\n");
}
