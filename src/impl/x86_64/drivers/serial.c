#include <drivers/serial.h>
#include <interrupts/port_io.h>

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);  // Disable interrupts
    outb(COM1 + 3, 0x80);  // Set DLAB
    outb(COM1 + 0, 0x03);  // Divisor latch LSB (38400 baud)
    outb(COM1 + 1, 0x00);  // Divisor latch MSB
    outb(COM1 + 3, 0x03);  // 8 bits, 1 stop bit, no parity
    outb(COM1 + 2, 0xC7);  // FIFO control
    outb(COM1 + 4, 0x0B);  // RTS/DTR handshake
}

void serial_putchar(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, c);
}

void serial_print(const char *str) {
    while (*str)
        serial_putchar(*str++);
}

void serial_print_hex(uint64_t value) {
    char hex[] = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4)
        serial_putchar(hex[(value >> i) & 0xF]);
}

void serial_print_dec(uint32_t value) {
    char buffer[32];
    int i = 0;

    if (value == 0) {
        serial_putchar('0');
        return;
    }

    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
        serial_putchar(buffer[--i]);
}
