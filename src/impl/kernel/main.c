#include <graphics/graphics.h>
#include <shell/shell.h>

void kernel_main(void)
{
    // Initialize graphics
    graphics_init();

    init_interrupts_safe();
    keyboard_init();

    enable_interrupts();

    // Draw a welcome screen
    uint32_t cx = get_screen_width() / 2;
    uint32_t cy = get_screen_height() / 2;

    draw_triangle(cx, cy - 100, cx - 100, cy + 100, cx + 100, cy + 100, COLOR_CYAN);
    fill_rect(10, 10, 200, 50, COLOR_BLUE);

    // Start your shell
    shell_run();
}