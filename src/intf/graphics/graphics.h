#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

// Initialize graphics system
void graphics_init(void);

// Basic drawing functions
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear_screen(uint32_t color);
void fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);

// Get screen info
uint32_t get_screen_width(void);
uint32_t get_screen_height(void);

// Common colors
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_CYAN 0x00FFFF
#define COLOR_MAGENTA 0xFF00FF

#endif