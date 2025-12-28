#include <graphics/graphics.h>
#include <stddef.h>

extern uint64_t framebuffer_address;
extern uint32_t screen_width;
extern uint32_t screen_height;
extern uint32_t bits_per_pixel;
extern uint32_t pitch;

static uint32_t *framebuffer = NULL;

void graphics_init(void)
{
    framebuffer = (uint32_t *)(uintptr_t)framebuffer_address;
    clear_screen(COLOR_BLACK);
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= screen_width || y >= screen_height)
        return;
    framebuffer[y * (pitch / 4) + x] = color;
}

void clear_screen(uint32_t color)
{
    uint32_t total_pixels = (pitch / 4) * screen_height;
    for (uint32_t i = 0; i < total_pixels; i++)
    {
        framebuffer[i] = color;
    }
}

void fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t dy = 0; dy < h; dy++)
    {
        for (uint32_t dx = 0; dx < w; dx++)
        {
            put_pixel(x + dx, y + dy, color);
        }
    }
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = (dx > dy ? dx : dy);
    if (steps < 0)
        steps = -steps;

    if (steps == 0)
    {
        put_pixel(x0, y0, color);
        return;
    }

    for (int i = 0; i <= steps; i++)
    {
        int x = x0 + (dx * i) / steps;
        int y = y0 + (dy * i) / steps;
        put_pixel(x, y, color);
    }
}

void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    draw_line(x0, y0, x1, y1, color);
    draw_line(x1, y1, x2, y2, color);
    draw_line(x2, y2, x0, y0, color);
}

uint32_t get_screen_width(void)
{
    return screen_width;
}

uint32_t get_screen_height(void)
{
    return screen_height;
}