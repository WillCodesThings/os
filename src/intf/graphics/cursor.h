#pragma once

#include <stdint.h>

// Cursor states
typedef enum {
    CURSOR_ARROW = 0,
    CURSOR_MOVE,
    CURSOR_HAND
} cursor_state_t;

void cursor_init(void);

void cursor_update(int32_t x, int32_t y);

void cursor_draw(void);

void cursor_hide(void);

void set_cursor_color(uint32_t color);

void set_cursor_state(cursor_state_t state);

cursor_state_t get_cursor_state(void);

void cursor_show(void);
