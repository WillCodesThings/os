#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

enum
{
    PRINT_COLOR_BLACK = 0,
    PRINT_COLOR_BLUE = 1,
    PRINT_COLOR_GREEN = 2,
    PRINT_COLOR_CYAN = 3,
    PRINT_COLOR_RED = 4,
    PRINT_COLOR_MAGENTA = 5,
    PRINT_COLOR_BROWN = 6,
    PRINT_COLOR_LIGHT_GRAY = 7,
    PRINT_COLOR_DARK_GRAY = 8,
    PRINT_COLOR_LIGHT_BLUE = 9,
    PRINT_COLOR_LIGHT_GREEN = 10,
    PRINT_COLOR_LIGHT_CYAN = 11,
    PRINT_COLOR_LIGHT_RED = 12,
    PRINT_COLOR_PINK = 13,
    PRINT_COLOR_YELLOW = 14,
    PRINT_COLOR_WHITE = 15,
};

void print_init(void);
void clear_row(size_t row);
void print_clear(void);
void print_char(char c);
void print_str(char *str);
void print_set_color(char foreground, char background);
void printf(const char *format, ...);
void print_hex(uint64_t value);
void print_int(int num);
void print_uint(uint32_t num);
void print_set_cursor(size_t row, size_t col);
uint32_t print_get_cursor_x(void);
uint32_t print_get_cursor_y(void);
void print_move_cursor_left(void);
void print_move_cursor_right(void);

// Output redirection for piping
void print_redirect_to_buffer(char *buffer, int max_size);
void print_redirect_disable(void);
int print_redirect_get_length(void);