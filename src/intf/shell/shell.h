#pragma once

#include <stdint.h>

// Shell initialization
void shell_init(void);

// Process a single character input (int to allow extended key codes > 127)
void shell_process_char(int c);

// Main shell loop - call this from kernel_main
void shell_run(void);
