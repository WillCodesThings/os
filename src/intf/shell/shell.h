#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

// Shell initialization
void shell_init(void);

// Process a single character input
void shell_process_char(char c);

// Main shell loop - call this from kernel_main
void shell_run(void);

#endif // SHELL_H