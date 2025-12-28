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

uint32_t get_pixel_color(uint32_t x, uint32_t y)
{
    if (x >= screen_width || y >= screen_height)
        return 0;
    uint32_t color = framebuffer[y * (pitch / 4) + x];

    return color;
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

void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color, int isFilled)
{
    if (isFilled == 1)
    {
        // draw filled triangle using barycentric coordinates
        int minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
        int maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
        int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
        int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                int w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
                int w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
                int w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);

                if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
                {
                    put_pixel(x, y, color);
                }
            }
        }
    }
    else
    {
        draw_line(x0, y0, x1, y1, color);
        draw_line(x1, y1, x2, y2, color);
        draw_line(x2, y2, x0, y0, color);
    }
}

void draw_circle(int cx, int cy, int radius, uint32_t color, int isFilled)
{
    if (isFilled == 1)
    {
        for (int y = -radius; y <= radius; y++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                if (x * x + y * y <= radius * radius)
                {
                    put_pixel(cx + x, cy + y, color);
                }
            }
        }
        return;
    }

    // Midpoint circle algorithm for outline
    int x = radius;
    int y = 0;
    int decisionOver2 = 1 - x; // Decision criterion divided by 2 evaluated at x=r, y=0

    while (y <= x)
    {
        put_pixel(cx + x, cy + y, color);
        put_pixel(cx + y, cy + x, color);
        put_pixel(cx - x, cy + y, color);
        put_pixel(cx - y, cy + x, color);
        put_pixel(cx - x, cy - y, color);
        put_pixel(cx - y, cy - x, color);
        put_pixel(cx + x, cy - y, color);
        put_pixel(cx + y, cy - x, color);
        y++;
        if (decisionOver2 <= 0)
        {
            decisionOver2 += 2 * y + 1; // Change in decision criterion for y -> y+1
        }
        else
        {
            x--;
            decisionOver2 += 2 * y - 2 * x + 1; // Change for y -> y+1, x -> x-1
        }
    }
}

uint32_t get_screen_width(void)
{
    return screen_width;
}

uint32_t get_screen_height(void)
{
    return screen_height;
}