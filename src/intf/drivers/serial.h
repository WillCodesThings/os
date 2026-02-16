#pragma once

#include <stdint.h>

// Initialize COM1 serial port at 115200 baud
void serial_init(void);

// Write a single character to COM1
void serial_putchar(char c);

// Write a null-terminated string to COM1
void serial_print(const char *str);

// Write a 64-bit value as hexadecimal to COM1
void serial_print_hex(uint64_t value);

// Write a 32-bit value as decimal to COM1
void serial_print_dec(uint32_t value);
