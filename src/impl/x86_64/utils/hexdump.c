#include <utils/hexdump.h>
#include <drivers/serial.h>

static const char hex_chars[] = "0123456789abcdef";

static void print_hex_fixed(uint64_t val, int nibbles) {
    for (int i = (nibbles - 1) * 4; i >= 0; i -= 4) {
        serial_putchar(hex_chars[(val >> i) & 0xF]);
    }
}

void hexdump(const void *addr, uint32_t len) {
    const uint8_t *p = (const uint8_t *)addr;

    for (uint32_t offset = 0; offset < len; offset += 16) {
        // Address column
        print_hex_fixed((uint64_t)(uintptr_t)(p + offset), 16);
        serial_print("  ");

        // Hex bytes: first group of 8
        for (int i = 0; i < 8; i++) {
            if (offset + i < len) {
                serial_putchar(hex_chars[p[offset + i] >> 4]);
                serial_putchar(hex_chars[p[offset + i] & 0xF]);
                serial_putchar(' ');
            } else {
                serial_print("   ");
            }
        }
        serial_putchar(' ');

        // Hex bytes: second group of 8
        for (int i = 8; i < 16; i++) {
            if (offset + i < len) {
                serial_putchar(hex_chars[p[offset + i] >> 4]);
                serial_putchar(hex_chars[p[offset + i] & 0xF]);
                serial_putchar(' ');
            } else {
                serial_print("   ");
            }
        }

        // ASCII column
        serial_print(" |");
        for (int i = 0; i < 16; i++) {
            if (offset + i < len) {
                uint8_t c = p[offset + i];
                serial_putchar((c >= 0x20 && c < 0x7F) ? c : '.');
            } else {
                serial_putchar(' ');
            }
        }
        serial_print("|\n");
    }
}

void hexdump_log(log_level_t level, const char *tag, const void *addr, uint32_t len) {
    log_write(level, tag, "Hexdump at %p, %u bytes:", addr, len);
    hexdump(addr, len);
}
