#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

// Shell initialization
void shell_init(void);

// Process a single character input
void shell_process_char(char c);

void serial_print_hex(uint64_t value);
void serial_print(const char *str);

static uint8_t strlen(const char *s);
static int strcmp(const char *s1, const char *s2);

// Main shell loop - call this from kernel_main
void shell_run(void);

#endif // SHELL_H
