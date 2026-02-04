#include <stdint.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <interrupts/port_io.h>
#include <interrupts/io/keyboard.h>
#include <shell/print.h>

// Forward declaration of the assembly keyboard handler
extern void keyboard_interrupt_handler(void);

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_IRQ 1
#define KB_BUFFER_SIZE 64

static volatile uint8_t kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_buffer_head = 0;
static volatile int kb_buffer_tail = 0;

static volatile int shift_pressed = 0;
static volatile int ctrl_pressed = 0;
static volatile int alt_pressed = 0;
static volatile int caps_lock = 0;
static volatile int num_lock = 0;
static volatile int extended_scancode = 0;

// Simple US QWERTY keymap
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '};

static const char shift_scancode_to_ascii[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
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
    // Handle extended scancode prefix (0xE0)
    if (scancode == 0xE0)
    {
        extended_scancode = 1;
        return;
    }

    // Handle extended scancodes (arrow keys, etc.)
    if (extended_scancode)
    {
        extended_scancode = 0;

        // Only process key press (not release)
        if (!(scancode & 0x80))
        {
            switch (scancode)
            {
            case 0x48: // Up arrow
                kb_buffer_put(KEY_UP);
                return;
            case 0x50: // Down arrow
                kb_buffer_put(KEY_DOWN);
                return;
            case 0x4B: // Left arrow
                kb_buffer_put(KEY_LEFT);
                return;
            case 0x4D: // Right arrow
                kb_buffer_put(KEY_RIGHT);
                return;
            case 0x47: // Home
                kb_buffer_put(KEY_HOME);
                return;
            case 0x4F: // End
                kb_buffer_put(KEY_END);
                return;
            case 0x53: // Delete
                kb_buffer_put(KEY_DELETE);
                return;
            case 0x49: // Page Up
                kb_buffer_put(KEY_PGUP);
                return;
            case 0x51: // Page Down
                kb_buffer_put(KEY_PGDN);
                return;
            case 0x52: // Insert
                kb_buffer_put(KEY_INSERT);
                return;
            }
        }
        return;
    }

    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = 1; // Shift pressed
    }
    else if (scancode == 0xAA || scancode == 0xB6)
    {
        shift_pressed = 0; // Shift released
    }
    else if (scancode == 0x1D)
    {
        ctrl_pressed = 1; // Ctrl pressed
    }
    else if (scancode == 0x9D)
    {
        ctrl_pressed = 0; // Ctrl released
    }
    else if (scancode == 0x38)
    {
        alt_pressed = 1; // Alt pressed
    }
    else if (scancode == 0xB8)
    {
        alt_pressed = 0; // Alt released
    }
    else if (scancode == 0x3A)
    {
        caps_lock ^= 1; // Toggle caps lock
    }
    else if (scancode == 0x45)
    {
        num_lock ^= 1; // Toggle num lock
    }

    if (!(scancode & 0x80))
    { // Only on key press
        // Handle Ctrl+key combinations
        if (ctrl_pressed)
        {
            if (scancode == 0x1F) // S key
            {
                kb_buffer_put(0x13); // Ctrl+S (ASCII DC3)
                return;
            }
        }

        if (scancode < sizeof(scancode_to_ascii) &&
            scancode_to_ascii[scancode])
        {
            char c = scancode_to_ascii[scancode];

            if (shift_pressed)
            {
                c = shift_scancode_to_ascii[scancode];
            }

            if (caps_lock && c >= 'a' && c <= 'z')
            {
                c -= 0x20; // Convert to uppercase
            }

            kb_buffer_put(c);
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
