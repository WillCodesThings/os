#include <shell/print.h>

const static size_t NUM_COLS = 80;
const static size_t NUM_ROWS = 25;

struct Char
{
    uint8_t character;
    uint8_t color;
};

struct Char *buffer = (struct Char *)0xb8000;
size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_WHITE | PRINT_COLOR_BLACK << 4;

void clear_row(size_t row)
{
    struct Char empty = (struct Char){
        character : ' ',
        color : color
    };

    for (size_t col = 0; col < NUM_COLS; col++)
    {
        buffer[row * NUM_COLS + col] = empty;
    }
}

void print_clear()
{
    for (size_t row = 0; row < NUM_ROWS; row++)
    {
        clear_row(row);
    }
}

void print_newline()
{
    col = 0;
    if (row < NUM_ROWS - 1)
    {
        row++;
        return;
    }

    for (size_t row = 1; row < NUM_ROWS; row++)
    {
        for (size_t col = 0; col < NUM_COLS; col++)
        {
            struct Char character = buffer[row * NUM_COLS + col];
            buffer[(row - 1) * NUM_COLS + col] = character;
        }
    }

    clear_row(NUM_ROWS - 1);
}

void print_set_cursor(size_t new_row, size_t new_col)
{
    if (new_row < NUM_ROWS && new_col < NUM_COLS)
    {
        row = new_row;
        col = new_col;
    }
}

void print_char(char c)
{
    if (c == '\n')
    {
        print_newline();
        return;
    }

    if (col >= NUM_COLS)
    {
        print_newline();
    }

    buffer[row * NUM_COLS + col] = (struct Char){
        character : c,
        color : color
    };

    col++;
}

void print_str(char *str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
    {
        char character = (uint8_t)str[i];

        print_char(character);
    }
}

void print_set_color(char foreground, char background)
{
    color = foreground + (background << 4);
}

char *itoa(int value, int base)
{
    static char buffer[32];
    char *ptr = &buffer[31];
    *ptr = '\0';

    int is_negative = (value < 0 && base == 10);
    if (is_negative)
    {
        value = -value;
    }

    do
    {
        int digit = value % base;
        *--ptr = (digit < 10) ? '0' + digit : 'A' + (digit - 10);
        value /= base;
    } while (value);

    if (is_negative)
    {
        *--ptr = '-';
    }

    return ptr;
}

void printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    for (size_t i = 0; format[i] != '\0'; i++)
    {
        if (format[i] == '%')
        {
            i++; // Move past '%'

            // Parse width specifier (optional)
            int width = 0;
            while (format[i] >= '0' && format[i] <= '9')
            {
                width = width * 10 + (format[i] - '0');
                i++;
            }

            // Handle format specifier
            switch (format[i])
            {
            case 'c':
            { // Character
                char c = (char)va_arg(args, int);
                if (width > 1)
                {
                    for (int pad = 0; pad < width - 1; pad++)
                        print_char(' ');
                }
                print_char(c);
                break;
            }
            case 's':
            { // String
                const char *str = va_arg(args, const char *);
                int len = 0;
                while (str[len] != '\0')
                    len++; // Calculate string length
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str(str);
                break;
            }
            case 'd':
            { // Decimal number
                int num = va_arg(args, int);
                char *num_str = itoa(num, 10); // Convert number to string
                int len = 0;
                while (num_str[len] != '\0')
                    len++; // Calculate string length
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str(num_str);
                break;
            }
            case 'x':
            case 'X':
            { // Hexadecimal number
                int num = va_arg(args, int);
                char *hex_str = itoa(num, 16); // Convert number to hex string
                int len = 0;
                while (hex_str[len] != '\0')
                    len++; // Calculate string length
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str(hex_str);
                break;
            }
            case 'p':
            { // Pointer
                uintptr_t ptr = va_arg(args, uintptr_t);
                print_str("0x");
                char *ptr_str = itoa(ptr, 16); // Convert pointer to string
                int len = 0;
                while (ptr_str[len] != '\0')
                    len++; // Calculate string length
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str(ptr_str);
                break;
            }
            case '%': // Literal '%'
                print_char('%');
                break;
            default: // Unsupported format specifier
                print_char('%');
                print_char(format[i]);
                break;
            }
        }
        else
        {
            print_char(format[i]); // Print non-format characters
        }
    }

    va_end(args);
}