#include <graphics/graphics.h>
#include <interrupts/safeInterrupt.h>
#include <interrupts/io/keyboard.h>
#include <interrupts/io/mouse.h>
#include <graphics/cursor.h>
#include <shell/print.h>
#include <shell/shell.h>
#include <fs/vfs.h>
#include <fs/simplefs.h>
#include <disk/block_device.h>

void kernel_main(void)
{
    // Initialize graphics
    graphics_init();
    print_init();

    init_interrupts_safe();
    // test_interrupts();

    keyboard_init();
    mouse_init();

    cursor_init();
    cursor_show();

    enable_interrupts();

    vfs_init();
    // tmpfs_init();
    block_device_t *device = get_block_device();

    simplefs_init(device);
    simplefs_mount(device);
    simplefs_format(device, 1024, 10);

    simplefs_test_setup();

    // debug_print_interrupt(0x2C); // IRQ12

    // Draw a welcome screen
    uint32_t cx = get_screen_width() / 2;
    uint32_t cy = get_screen_height() / 2;

    draw_triangle(cx, cy - 100, cx - 100, cy + 100, cx + 100, cy + 100, COLOR_CYAN, 1);
    // draw_triangle_3colors(cx, cy - 80, cx - 80, cy + 80, cx + 80, cy + 80, COLOR_MAGENTA, COLOR_YELLOW, COLOR_GREEN);
    fill_rect(10, 10, 200, 50, COLOR_BLUE);

    // mouse_test_polling();

    shell_run();
}
