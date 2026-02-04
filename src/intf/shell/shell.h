#pragma once

#include <stdint.h>

// Shell initialization
void shell_init(void);

// Process a single character input (int to allow extended key codes > 127)
void shell_process_char(int c);

void serial_print_hex(uint64_t value);
void serial_print_dec(uint32_t value);
void serial_print(const char *str);
void serial_putchar(char c);

// Main shell loop - call this from kernel_main
void shell_run(void);
