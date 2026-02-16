#include <utils/log.h>
#include <drivers/serial.h>
#include <stdarg.h>

#define ANSI_RESET "\033[0m"
#define ANSI_GRAY "\033[90m"
#define ANSI_CYAN "\033[36m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"
#define ANSI_BOLD "\033[1m"

static log_level_t min_level = LOG_TRACE;

static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR",
};

static const char *level_colors[] = {
    ANSI_GRAY,   // TRACE
    ANSI_CYAN,   // DEBUG
    ANSI_GREEN,  // INFO
    ANSI_YELLOW, // WARN
    ANSI_RED,    // ERROR
};

void log_init(void) {
    serial_init();
    min_level = LOG_TRACE;
    serial_print("\n");
    serial_print(ANSI_BOLD ANSI_GREEN);
    serial_print("========================================\n");
    serial_print("  Kernel Logger Initialized\n");
    serial_print("========================================\n");
    serial_print(ANSI_RESET "\n");
}

void log_set_level(log_level_t level) {
    min_level = level;
}

static void log_print_dec(uint64_t value) {
    char buffer[20];
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

static void log_print_hex(uint64_t value) {
    const char hex[] = "0123456789abcdef";
    serial_print("0x");

    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int nibble = (value >> i) & 0xF;
        if (nibble != 0 || started || i == 0) {
            serial_putchar(hex[nibble]);
            started = 1;
        }
    }
}

static void log_vprintf(const char *fmt, va_list args) {
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
            case 's': {
                const char *s = va_arg(args, const char *);
                serial_print(s ? s : "(null)");
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    serial_putchar('-');
                    val = -val;
                }
                log_print_dec((uint64_t)val);
                break;
            }
            case 'u': {
                uint32_t val = va_arg(args, uint32_t);
                log_print_dec(val);
                break;
            }
            case 'x':
            case 'X': {
                uint32_t val = va_arg(args, uint32_t);
                log_print_hex(val);
                break;
            }
            case 'p': {
                uintptr_t val = va_arg(args, uintptr_t);
                log_print_hex(val);
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                serial_putchar(c);
                break;
            }
            case '%':
                serial_putchar('%');
                break;
            default:
                serial_putchar('%');
                serial_putchar(fmt[i]);
                break;
            }
        } else {
            serial_putchar(fmt[i]);
        }
    }
}

void log_write(log_level_t level, const char *tag, const char *fmt, ...) {
    if (level < min_level)
        return;

    serial_print(level_colors[level]);
    serial_print(level_names[level]);
    serial_print(ANSI_RESET);
    serial_print(" [");
    serial_print(tag);
    serial_print("] ");

    va_list args;
    va_start(args, fmt);
    log_vprintf(fmt, args);
    va_end(args);

    serial_putchar('\n');
}
