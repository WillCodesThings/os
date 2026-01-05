#include <interrupts/io/mouse.h>
#include <interrupts/idt.h>
#include <interrupts/port_io.h>
#include <shell/shell.h>

// Forward declaration of assembly handler
extern void mouse_interrupt_handler(void);

#define MOUSE_PORT 0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD 0x64
#define MOUSE_IRQ 12

static mouse_state_t mouse_state = {0};
static volatile uint8_t mouse_cycle = 0;
static volatile int8_t mouse_bytes[3];

// volatile uint32_t mouse_interrupt_count = 0;

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
    // mouse_interrupt_count++;
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
    serial_print("Initializing mouse...\n");

    uint8_t status;

    // Disable devices during setup
    mouse_wait_write();
    outb(MOUSE_CMD, 0xAD); // Disable keyboard
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA7); // Disable mouse

    // Flush output buffer
    inb(MOUSE_PORT);

    // Enable auxiliary device
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);
    serial_print("Enabled auxiliary device\n");

    // Get controller configuration byte
    mouse_wait_write();
    outb(MOUSE_CMD, 0x20);
    mouse_wait_read();
    status = inb(MOUSE_PORT);
    serial_print("Controller config: ");
    serial_print_hex(status);
    serial_print("\n");

    // Enable interrupts and mouse clock
    status |= 0x02;  // Enable mouse interrupt
    status |= 0x01;  // Enable keyboard interrupt
    status &= ~0x10; // Enable mouse clock
    status &= ~0x20; // Enable keyboard clock

    // Write back configuration
    mouse_wait_write();
    outb(MOUSE_CMD, 0x60);
    mouse_wait_write();
    outb(MOUSE_PORT, status);

    // Reset mouse
    serial_print("Resetting mouse...\n");
    mouse_write(0xFF);
    mouse_wait_read();
    uint8_t reset_ack = inb(MOUSE_PORT);
    serial_print("Reset ACK: ");
    serial_print_hex(reset_ack);
    serial_print("\n");

    // Should receive 0xAA (self-test passed) and 0x00 (mouse ID)
    mouse_wait_read();
    uint8_t self_test = inb(MOUSE_PORT);
    serial_print("Self-test: ");
    serial_print_hex(self_test);
    serial_print("\n");

    if (self_test == 0xAA)
    {
        mouse_wait_read();
        uint8_t mouse_id = inb(MOUSE_PORT);
        serial_print("Mouse ID: ");
        serial_print_hex(mouse_id);
        serial_print("\n");
    }

    // Set defaults
    mouse_write(0xF6);
    mouse_wait_read();
    uint8_t defaults_ack = inb(MOUSE_PORT);
    serial_print("Defaults ACK: ");
    serial_print_hex(defaults_ack);
    serial_print("\n");

    // Enable data reporting
    mouse_write(0xF4);
    mouse_wait_read();
    uint8_t enable_ack = inb(MOUSE_PORT);
    serial_print("Enable ACK: ");
    serial_print_hex(enable_ack);
    serial_print("\n");

    // Re-enable devices
    mouse_wait_write();
    outb(MOUSE_CMD, 0xAE); // Re-enable keyboard

    // Initialize state
    extern uint32_t screen_width;
    extern uint32_t screen_height;

    mouse_state.x = screen_width / 2;
    mouse_state.y = screen_height / 2;
    mouse_state.buttons = 0;
    mouse_cycle = 0;

    // Register interrupt handler
    serial_print("Registering interrupt handler at 0x2C\n");
    idt_set_gate(0x2C, (uint64_t)mouse_interrupt_handler);

    // Unmask IRQ12 on follower PIC
    uint8_t follower_mask = inb(0xA1);
    follower_mask &= ~(1 << 4);
    outb(0xA1, follower_mask);

    // Unmask IRQ2 on master PIC (cascade)
    uint8_t master_mask = inb(0x21);
    master_mask &= ~(1 << 2);
    outb(0x21, master_mask);

    serial_print("Mouse initialized!\n");
}

// Add this test function
void mouse_test_polling(void)
{
    serial_print("Testing mouse with polling...\n");

    for (int i = 0; i < 10; i++)
    {
        // Wait a bit
        for (volatile int j = 0; j < 10000000; j++)
            ;

        // Check if data is available
        uint8_t status = inb(MOUSE_STATUS);
        serial_print("Status: ");
        serial_print_hex(status);

        if (status & 0x01)
        {
            uint8_t data = inb(MOUSE_PORT);
            serial_print(" Data: ");
            serial_print_hex(data);
        }
        serial_print("\n");
    }
}

mouse_state_t mouse_get_state(void)
{
    return mouse_state;
}