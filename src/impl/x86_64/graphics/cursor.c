#include <graphics/cursor.h>
#include <graphics/graphics.h>

#define CURSOR_WIDTH 11
#define CURSOR_HEIGHT 16

static int32_t cursor_x = 0;
static int32_t cursor_y = 0;
static uint32_t saved_background[CURSOR_WIDTH * CURSOR_HEIGHT];
static uint8_t cursor_visible = 1;

// Simple arrow cursor bitmap
static const uint8_t cursor_bitmap[CURSOR_HEIGHT] = {
    0b10000000, // *
    0b11000000, // **
    0b11100000, // ***
    0b11110000, // ****
    0b11111000, // *****
    0b11111100, // ******
    0b11111110, // *******
    0b11111111, // ********
    0b11111000, // *****
    0b11011000, // ** **
    0b10001100, // *   **
    0b00001100, //    **
    0b00000110, //     **
    0b00000110, //     **
    0b00000011, //      **
    0b00000000, //
};

void cursor_init(void)
{
    cursor_x = get_screen_width() / 2;
    cursor_y = get_screen_height() / 2;
}

void cursor_hide(void)
{
    if (!cursor_visible)
        return;

    // Restore saved background
    for (int y = 0; y < CURSOR_HEIGHT; y++)
    {
        for (int x = 0; x < CURSOR_WIDTH; x++)
        {
            int screen_x = cursor_x + x;
            int screen_y = cursor_y + y;

            if (screen_x >= 0 && screen_x < (int)get_screen_width() &&
                screen_y >= 0 && screen_y < (int)get_screen_height())
            {
                put_pixel(screen_x, screen_y, saved_background[y * CURSOR_WIDTH + x]);
            }
        }
    }

    cursor_visible = 0;
}

void cursor_draw(void)
{
    // Save background first
    for (int y = 0; y < CURSOR_HEIGHT; y++)
    {
        for (int x = 0; x < CURSOR_WIDTH; x++)
        {
            int screen_x = cursor_x + x;
            int screen_y = cursor_y + y;

            if (screen_x >= 0 && screen_x < (int)get_screen_width() &&
                screen_y >= 0 && screen_y < (int)get_screen_height())
            {
                // We need a get_pixel function - for now just use black
                saved_background[y * CURSOR_WIDTH + x] = 0x000000;
            }
        }
    }

    // Draw cursor
    for (int y = 0; y < CURSOR_HEIGHT; y++)
    {
        uint8_t row = cursor_bitmap[y];
        for (int x = 0; x < 8; x++)
        {
            if (row & (0x80 >> x))
            {
                int screen_x = cursor_x + x;
                int screen_y = cursor_y + y;

                if (screen_x >= 0 && screen_x < (int)get_screen_width() &&
                    screen_y >= 0 && screen_y < (int)get_screen_height())
                {
                    // Draw white cursor with black outline
                    put_pixel(screen_x, screen_y, 0xFFFFFF);
                }
            }
        }
    }

    cursor_visible = 1;
}

void cursor_update(int32_t x, int32_t y)
{
    // Hide cursor at old position
    cursor_hide();

    // Update position
    cursor_x = x;
    cursor_y = y;

    // Draw at new position
    cursor_draw();
}

void cursor_show(void)
{
    cursor_draw();
}