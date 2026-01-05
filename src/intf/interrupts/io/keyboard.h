#pragma once 

#include <stdint.h>

// Initialize the keyboard subsystem
void keyboard_init();

// Check if a key is available in the keyboard buffer
int keyboard_available();

// Read a character from the keyboard buffer
int keyboard_read();

void keyboard_late_init();

// Enable keyboard processing
void keyboard_enable_processing();
