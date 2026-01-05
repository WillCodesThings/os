

#include <interrupts/safeInterrupt.h>
#include <interrupts/io/keyboard.h>
#include <interrupts/io/mouse.h>
#include <interrupts/io/ata.h>

#include <graphics/graphics.h>
#include <graphics/cursor.h>

#include <shell/print.h>
#include <shell/shell.h>

#include <disk/partition.h>
#include <disk/partition_block_device.h>
#include <disk/test.h>

#include <fs/vfs.h>
#include <fs/vfs_mount.h>
#include <fs/simplefs.h>

#include <memory/heap.h>

#include <init/inits.h>

void kernel_main(void)
{
    init_interrupts_safe();

    // test_interrupts();

    keyboard_init();
    mouse_init();
    ata_init();

    kernel_filesystem_init();

    // Create sample files for testing
    simplefs_create_sample_files();

    enable_interrupts();

    // Initialize graphics
    graphics_init();
    print_init();
    cursor_init();
    cursor_show();

    // Draw a welcome screen
    uint32_t cx = get_screen_width() / 2;
    uint32_t cy = get_screen_height() / 2;

    draw_triangle(cx, cy - 100, cx - 100, cy + 100, cx + 100, cy + 100, COLOR_CYAN, 1);
    // draw_triangle_3colors(cx, cy - 80, cx - 80, cy + 80, cx + 80, cy + 80, COLOR_MAGENTA, COLOR_YELLOW, COLOR_GREEN);
    fill_rect(10, 10, 200, 50, COLOR_BLUE);

    // kernel_test_filesystem();

    // mouse_test_polling();

    shell_run();
}