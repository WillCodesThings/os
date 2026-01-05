#include <graphics/cursor.h>
#include <graphics/graphics.h>

#define CURSOR_WIDTH 16
#define CURSOR_HEIGHT 16

static int32_t cursor_x = 0;
static int32_t cursor_y = 0;
static uint32_t saved_background[CURSOR_WIDTH * CURSOR_HEIGHT];
static uint8_t cursor_visible = 1;
static cursor_state_t cursor_state = CURSOR_ARROW;

uint32_t cursor_color = 0xFFFFFF; // White

// Arrow cursor bitmap (default)
static const uint8_t cursor_arrow[CURSOR_HEIGHT] = {
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

// Move cursor bitmap (4-way arrow)
static const uint16_t cursor_move[CURSOR_HEIGHT] = {
    0b0000001000000000, //    *
    0b0000011100000000, //   ***
    0b0000111110000000, //  *****
    0b0000001000000000, //    *
    0b0010001000100000, // *  *  *
    0b0110001000110000, //**  *  **
    0b1111111111110000, //***********
    0b0110001000110000, //**  *  **
    0b0010001000100000, // *  *  *
    0b0000001000000000, //    *
    0b0000111110000000, //  *****
    0b0000011100000000, //   ***
    0b0000001000000000, //    *
    0b0000000000000000, //
    0b0000000000000000, //
    0b0000000000000000, //
};

// Hand/pointer cursor bitmap
static const uint8_t cursor_hand[CURSOR_HEIGHT] = {
    0b00001100, //    **
    0b00010010, //   *  *
    0b00010010, //   *  *
    0b00010010, //   *  *
    0b00010010, //   *  *
    0b00010110, //   * **
    0b01110110, // *** **
    0b10011110, //*  ****
    0b10001110, //*   ***
    0b10001110, //*   ***
    0b01000110, // *   **
    0b01000110, // *   **
    0b00100010, //  *   *
    0b00100010, //  *   *
    0b00011100, //   ***
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
                saved_background[y * CURSOR_WIDTH + x] = get_pixel_color(screen_x, screen_y);
            }
        }
    }

    // Draw cursor based on current state
    for (int y = 0; y < CURSOR_HEIGHT; y++)
    {
        int width = (cursor_state == CURSOR_MOVE) ? 12 : 8;
        for (int x = 0; x < width; x++)
        {
            int pixel_set = 0;

            if (cursor_state == CURSOR_MOVE) {
                pixel_set = cursor_move[y] & (0x8000 >> x);
            } else if (cursor_state == CURSOR_HAND) {
                pixel_set = cursor_hand[y] & (0x80 >> x);
            } else {
                pixel_set = cursor_arrow[y] & (0x80 >> x);
            }

            if (pixel_set)
            {
                int screen_x = cursor_x + x;
                int screen_y = cursor_y + y;

                if (screen_x >= 0 && screen_x < (int)get_screen_width() &&
                    screen_y >= 0 && screen_y < (int)get_screen_height())
                {
                    put_pixel(screen_x, screen_y, cursor_color);
                }
            }
        }
    }

    cursor_visible = 1;
}

void set_cursor_color(uint32_t color)
{
    cursor_color = color;
}

void set_cursor_state(cursor_state_t state)
{
    if (cursor_state != state) {
        cursor_hide();
        cursor_state = state;
        cursor_draw();
    }
}

cursor_state_t get_cursor_state(void)
{
    return cursor_state;
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