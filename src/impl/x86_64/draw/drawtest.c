#include <stdint.h>
#include <draw/drawtest.h>

// Access the variables declared in assembly
extern volatile uint32_t framebuffer_address;
extern volatile uint16_t screen_width;
extern volatile uint16_t screen_height;
extern volatile uint8_t bits_per_pixel;
extern volatile uint16_t pitch;

// This points to the mode info structure that was filled by the BIOS
volatile VbeModeInfo *vbe_mode_info = (volatile VbeModeInfo *)0x00090000;

void get_mode_info(void)
{
    // The actual mode info was already retrieved in assembly
    // We don't need to do anything here, as the variables are already set
    // But we could validate them if needed

    if (framebuffer_address == 0 || screen_width == 0 ||
        screen_height == 0 || bits_per_pixel == 0)
    {
        // Error condition - mode info wasn't properly set
        // For debug purposes, you could set default values here

        // These are typical values for mode 0x118 (1024x768x32bpp)
        framebuffer_address = 0xA0000; // This will vary by hardware
        screen_width = 1024;
        screen_height = 768;
        bits_per_pixel = 32;
        pitch = screen_width * (bits_per_pixel / 8);
    }
}

void putpixel(uint32_t *framebuffer, int x, int y, int pitch, uint32_t color)
{
    if (x < 0 || y < 0 || x >= (int)screen_width || y >= (int)screen_height)
    {
        return; // Outside screen bounds
    }
    uint8_t *pixel = (uint8_t *)framebuffer + y * pitch + x * (bits_per_pixel / 8);
    *(uint32_t *)pixel = color;
}

void fillrect(uint32_t *framebuffer, int pitch, int x, int y, int w, int h,
              uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = (r << 16) | (g << 8) | b;

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            putpixel(framebuffer, x + col, y + row, pitch, color);
        }
    }
}