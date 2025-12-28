#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// Simple 8x8 bitmap font
extern const uint8_t font_8x8[128][8];

void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
void draw_string(const char *str, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);

#endif