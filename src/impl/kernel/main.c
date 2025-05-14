#include <stdint.h>
#include "print.h"
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "safeInterrupt.h"
#include "shell.h"

// Simple delay function
void delay(uint64_t ticks)
{
    for (uint64_t i = 0; i < ticks; i++)
    {
        // Simple delay loop
        asm volatile("nop");
    }
}

void kernel_main()
{
    // Print welcome message
    print_str("OS Kernel: Loading...\n");
    init_interrupts_safe();
    keyboard_init();

    enable_interrupts();

    print_str("\nKeyboard Ready! Type something:\n");

    // Run the shell
    shell_run();

    // This code should never be reached
    while (1)
    {
        // Idle loop
        asm volatile("hlt");
    }
}