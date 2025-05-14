#include <stdint.h>
#include "idt.h"
#include "pic.h"
#include "print.h"
#include "port_io.h"

// Declaration for the assembly-defined default handler
extern void default_interrupt_handler(void);

// C handler for default interrupts
void c_default_handler(int interrupt_num);

// Initializes IDT with safe default handlers for all interrupts
void setup_safe_idt()
{
    // Set up IDT structure
    idt_install();

    // Install default handler for all interrupts
    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        idt_set_gate(i, (uint64_t)default_interrupt_handler);
    }

    // Load the IDT
    idt_load();
}

extern void enable_interrupts();

void enable_interrupts()
{
    asm volatile("sti");
}

// Safe initialization of interrupt subsystem
void init_interrupts_safe()
{
    // Step 1: Initialize IDT with default handlers
    setup_safe_idt();

    // Step 2: Remap PIC to avoid conflicts with CPU exceptions
    // Map IRQs 0-7 to interrupts 0x20-0x27
    // Map IRQs 8-15 to interrupts 0x28-0x2F
    pic_remap(0x20, 0x28);

    // Step 3: Mask all interrupts initially
    // This prevents any hardware interrupts from firing
    outb(PIC1_DATA, 0xFF); // Disable all IRQs on master PIC
    outb(PIC2_DATA, 0xFF); // Disable all IRQs on slave PIC

    // Note: We do NOT enable interrupts here
    // This will be done in a controlled manner in test_interrupts()
}

// Function to test interrupts with careful control
void test_interrupts()
{
    // Print initial status
    print_str("Starting interrupt testing sequence...\n");

    // 1. First, enable CPU interrupts with all hardware interrupts masked
    print_str("Enabling CPU interrupts with all hardware masked...\n");
    asm volatile("sti");

    // Wait a bit to ensure system is stable
    print_str("Waiting to ensure stability...\n");
    for (volatile int i = 0; i < 1000000; i++)
        ;

    // 2. Now enable keyboard interrupt only
    print_str("Enabling keyboard interrupt only...\n");
    outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 1)); // Unmask IRQ1 (keyboard)

    // Wait again to ensure stability
    print_str("Waiting to verify keyboard interrupt works...\n");
    for (volatile int i = 0; i < 1000000; i++)
        ;

    // 3. Enable timer interrupt for system clock
    // Uncomment when keyboard is working reliably
    // print_str("Enabling timer interrupt...\n");
    // outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 0)); // Unmask IRQ0 (timer)

    print_str("Interrupt testing complete - interrupts enabled!\n");
}

// Debug function to print interrupt information
void debug_print_interrupt(int interrupt_num)
{
    char buffer[32];

    // Format: "INT: XX"
    buffer[0] = 'I';
    buffer[1] = 'N';
    buffer[2] = 'T';
    buffer[3] = ':';
    buffer[4] = ' ';

    // Convert interrupt number to string
    if (interrupt_num < 10)
    {
        buffer[5] = '0' + interrupt_num;
        buffer[6] = '\0';
    }
    else
    {
        buffer[5] = '0' + (interrupt_num / 10);
        buffer[6] = '0' + (interrupt_num % 10);
        buffer[7] = '\0';
    }

    print_str(buffer);
    print_str("\n");
}

// C handler for default interrupts
void c_default_handler(int interrupt_num)
{
    // Print which interrupt was triggered for debugging
    debug_print_interrupt(interrupt_num);

    // Send EOI as appropriate - crucial step!
    if (interrupt_num >= 0x20 && interrupt_num < 0x30)
    {
        // It's a PIC interrupt
        if (interrupt_num >= 0x28)
        {
            // It's from the slave PIC
            outb(PIC2_COMMAND, 0x20);
        }
        // Send EOI to master PIC for all hardware interrupts
        outb(PIC1_COMMAND, 0x20);
    }
}