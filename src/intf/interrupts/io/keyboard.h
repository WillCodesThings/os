#pragma once

#include <stdint.h>

// Special key codes (outside ASCII range)
#define KEY_UP      0x80
#define KEY_DOWN    0x81
#define KEY_LEFT    0x82
#define KEY_RIGHT   0x83
#define KEY_HOME    0x84
#define KEY_END     0x85
#define KEY_DELETE  0x86
#define KEY_PGUP    0x87
#define KEY_PGDN    0x88
#define KEY_INSERT  0x89
#define KEY_TAB     '\t'

// Initialize the keyboard subsystem
void keyboard_init();

// Check if a key is available in the keyboard buffer
int keyboard_available();

// Read a character from the keyboard buffer
int keyboard_read();

void keyboard_late_init();

// Enable keyboard processing
void keyboard_enable_processing();
