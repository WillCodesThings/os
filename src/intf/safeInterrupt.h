#ifndef INTERRUPTS_SAFE_H
#define INTERRUPTS_SAFE_H

#include <stdint.h>

// Assembly-defined default interrupt handler
extern void default_interrupt_handler(void);

// C handler for default interrupts
void c_default_handler(int interrupt_num);

// Debug function to print interrupt information
void debug_print_interrupt(int interrupt_num);

// Initializes IDT with safe default handlers for all interrupts
void setup_safe_idt(void);

// Safe initialization of interrupt subsystem
void init_interrupts_safe(void);

// Function to test interrupts with careful control
void test_interrupts(void);

#endif // INTERRUPTS_SAFE_H