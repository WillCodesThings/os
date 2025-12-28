#include <interrupts/io/mouse.h>
#include <interrupts/idt.h>
#include <interrupts/port_io.h>
#include <shell/print.h>

// Forward declaration of assembly handler
extern void mouse_interrupt_handler(void);

#define MOUSE_PORT 0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD 0x64
#define MOUSE_IRQ 12

static mouse_state_t mouse_state = {0};
static volatile uint8_t mouse_cycle = 0;
static volatile int8_t mouse_bytes[3];

// Wait for mouse to be ready to write
static void mouse_wait_write(void)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        if ((inb(MOUSE_STATUS) & 0x02) == 0)
        {
            return;
        }
    }
}

// Wait for mouse to be ready to read
static void mouse_wait_read(void)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        if (inb(MOUSE_STATUS) & 0x01)
        {
            return;
        }
    }
}

// Write to mouse
static void mouse_write(uint8_t data)
{
    mouse_wait_write();
    outb(MOUSE_CMD, 0xD4);
    mouse_wait_write();
    outb(MOUSE_PORT, data);
}

// Read from mouse
static uint8_t mouse_read(void)
{
    mouse_wait_read();
    return inb(MOUSE_PORT);
}

// This gets called from the ASM handler
void mouse_handle_interrupt(void)
{
    uint8_t status = inb(MOUSE_STATUS);

    if (!(status & 0x20))
    {
        // Not mouse data, ignore
        return;
    }

    uint8_t data = inb(MOUSE_PORT);

    switch (mouse_cycle)
    {
    case 0:
        // First byte: button status and overflow flags
        if (!(data & 0x08))
        {
            // Invalid packet (bit 3 should always be 1), skip
            return;
        }
        mouse_bytes[0] = data;
        mouse_cycle++;
        break;

    case 1:
        // Second byte: X movement
        mouse_bytes[1] = data;
        mouse_cycle++;
        break;

    case 2:
        // Third byte: Y movement
        mouse_bytes[2] = data;
        mouse_cycle = 0;

        // Process complete packet
        mouse_state.buttons = mouse_bytes[0] & 0x07;

        // Calculate movement (handle sign extension)
        int16_t dx = mouse_bytes[1];
        int16_t dy = mouse_bytes[2];

        if (mouse_bytes[0] & 0x10)
            dx |= 0xFF00; // Sign extend X
        if (mouse_bytes[0] & 0x20)
            dy |= 0xFF00; // Sign extend Y

        mouse_state.delta_x = dx;
        mouse_state.delta_y = dy;

        // Update position (invert Y for screen coordinates)
        mouse_state.x += dx;
        mouse_state.y -= dy;

        // Clamp to screen bounds
        extern uint32_t screen_width;
        extern uint32_t screen_height;

        if (mouse_state.x < 0)
            mouse_state.x = 0;
        if (mouse_state.y < 0)
            mouse_state.y = 0;
        if (mouse_state.x >= (int32_t)screen_width)
            mouse_state.x = screen_width - 1;
        if (mouse_state.y >= (int32_t)screen_height)
            mouse_state.y = screen_height - 1;

        break;
    }
}

void mouse_init(void)
{
    print_str("Initializing mouse...\n");

    // Enable auxiliary mouse device
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);

    // Get compaq status byte
    mouse_wait_write();
    outb(MOUSE_CMD, 0x20);
    mouse_wait_read();
    uint8_t status = inb(MOUSE_PORT);

    // Set compaq status byte (enable IRQ12)
    status |= 0x02;  // Enable IRQ12
    status &= ~0x20; // Enable mouse clock

    mouse_wait_write();
    outb(MOUSE_CMD, 0x60);
    mouse_wait_write();
    outb(MOUSE_PORT, status);

    // Use default settings
    mouse_write(0xF6);
    mouse_read(); // Acknowledge

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read(); // Acknowledge

    // Initialize mouse state
    extern uint32_t screen_width;
    extern uint32_t screen_height;

    mouse_state.x = screen_width / 2;
    mouse_state.y = screen_height / 2;
    mouse_state.buttons = 0;
    mouse_state.delta_x = 0;
    mouse_state.delta_y = 0;

    mouse_cycle = 0;

    // Register the mouse handler - IRQ12 is interrupt 0x2C (32 + 12)
    idt_set_gate(0x2C, (uint64_t)mouse_interrupt_handler);

    // Unmask IRQ12 in PIC (slave PIC)
    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4); // IRQ12 is bit 4 on slave PIC
    outb(0xA1, mask);

    print_str("Mouse initialized!\n");
}

mouse_state_t mouse_get_state(void)
{
    return mouse_state;
}