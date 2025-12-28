#ifndef CURSOR_H
#define CURSOR_H

#include <stdint.h>

void cursor_init(void);

void cursor_update(int32_t x, int32_t y);

void cursor_draw(void);

void cursor_hide(void);

void cursor_show(void);

#endif