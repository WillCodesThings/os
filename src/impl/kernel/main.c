#include <stdint.h>
#include <interrupts/io/keyboard.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <interrupts/safeInterrupt.h>
#include <shell/shell.h>
#include <shell/print.h>
// #include <draw/drawtest.h>

// Simple delay function
void delay(uint64_t ticks)
{
    for (uint64_t i = 0; i < ticks; i++)
    {
        // Simple delay loop
        asm volatile("nop");
    }
}

// extern volatile uint32_t framebuffer_address;
// extern volatile uint16_t pitch;

void kernel_main()
{

    // init_graphics();

    // Cast framebuffer physical address to pointer
    // Note: This assumes identity mapping or that the framebuffer is already mapped
    // uint32_t *fb = (uint32_t *)(uintptr_t)framebuffer_address;

    // // Draw a test pattern to confirm graphics are working

    // // Red rectangle at (100,100), size 200x100 pixels
    // fillrect(fb, pitch, 100, 100, 200, 100, 0xFF, 0x00, 0x00);

    // // Green rectangle at (400,100), size 200x100 pixels
    // fillrect(fb, pitch, 400, 100, 200, 100, 0x00, 0xFF, 0x00);

    // // Blue rectangle at (250,300), size 200x100 pixels
    // fillrect(fb, pitch, 250, 300, 200, 100, 0x00, 0x00, 0xFF);

    // For example:
    // init_interrupts_safe();
    // keyboard_init();
    // enable_interrupts();
    // shell_run();

    // Hang or continue with other kernel tasks
    while (1)
    {
        asm("hlt");
    }

    // Print welcome message
    print_str("OS Kernel: Loading...\n");
    init_interrupts_safe();
    keyboard_init();

    enable_interrupts();

    print_str("\nKeyboard Ready! Type something:\n");

    fillrect((unsigned char *)0xA0000, 0, 0, 0, 320, 200);
    // Run the shell
    shell_run();

    // This code should never be reached
    while (1)
    {
        // Idle loop
        asm volatile("hlt");
    }
}