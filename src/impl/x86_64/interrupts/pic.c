#include <stdint.h>
#include <interrupts/pic.h>
#include <interrupts/port_io.h>
#include <shell/print.h>

void pic_remap(int offset1, int offset2)
{
    // Save current masks
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    // Start initialization
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Set vector offsets
    outb(PIC1_DATA, offset1); // Master PIC IRQ0-7
    io_wait();
    outb(PIC2_DATA, offset2); // follower PIC IRQ8-15
    io_wait();

    // Setup cascade
    outb(PIC1_DATA, 4); // Tell Master PIC there’s a follower at IRQ2
    io_wait();
    outb(PIC2_DATA, 2); // Tell follower PIC its cascade identity
    io_wait();

    // Set 8086/88 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Restore saved masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}
