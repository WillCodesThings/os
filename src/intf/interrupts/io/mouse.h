#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// Mouse state structure
typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons;  // Bit 0: left, Bit 1: right, Bit 2: middle
    int8_t delta_x;
    int8_t delta_y;
} mouse_state_t;

// Initialize PS/2 mouse
void mouse_init(void);

// Get current mouse state
mouse_state_t mouse_get_state(void);

// Mouse button flags
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

#endif