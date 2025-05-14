#include <stdint.h>
#include "idt.h"
#include "pic.h"
#include "port_io.h"
#include "print.h"

// Forward declaration of the assembly keyboard handler
extern void keyboard_interrupt_handler(void);

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_IRQ 1
#define KB_BUFFER_SIZE 64

static volatile uint8_t kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_buffer_head = 0;
static volatile int kb_buffer_tail = 0;

// Simple US QWERTY keymap
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '};

// Circular buffer insert
static void kb_buffer_put(uint8_t c)
{
    int next = (kb_buffer_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_buffer_tail)
    {
        kb_buffer[kb_buffer_head] = c;
        kb_buffer_head = next;
    }
}

// Scancode processing
static void process_scancode(uint8_t scancode)
{
    if (!(scancode & 0x80))
    { // Only on key press
        if (scancode < sizeof(scancode_to_ascii))
        {
            char c = scancode_to_ascii[scancode];

            if (c)
            {
                kb_buffer_put(c);
            }
        }
    }
}

// This gets called from the ASM handler
void keyboard_handle_interrupt()
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    process_scancode(scancode);

    // EOI to PIC is handled in the assembly code
}

// Public: check if key is available
int keyboard_available()
{
    return kb_buffer_head != kb_buffer_tail;
}

// Public: read character from buffer
int keyboard_read()
{
    if (kb_buffer_head == kb_buffer_tail)
    {
        return 0; // No input
    }

    uint8_t c = kb_buffer[kb_buffer_tail];
    kb_buffer_tail = (kb_buffer_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

// Initialize keyboard
void keyboard_init()
{
    // Register the keyboard handler
    idt_set_gate(0x21, (uint64_t)keyboard_interrupt_handler);

    // Unmask IRQ1 (keyboard) in PIC
    outb(0x21, inb(0x21) & ~(1 << KEYBOARD_IRQ));
}
