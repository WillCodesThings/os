#include <shell/print.h>
#include <graphics/font.h>
#include <graphics/graphics.h>

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static const uint32_t CHAR_WIDTH = 8;
static const uint32_t CHAR_HEIGHT = 8;
static uint32_t max_cols = 0;
static uint32_t max_rows = 0;

// Color mapping from VGA colors to RGB
static const uint32_t vga_to_rgb[16] = {
    0x000000, // BLACK
    0x0000AA, // BLUE
    0x00AA00, // GREEN
    0x00AAAA, // CYAN
    0xAA0000, // RED
    0xAA00AA, // MAGENTA
    0xAA5500, // BROWN
    0xAAAAAA, // LIGHT_GRAY
    0x555555, // DARK_GRAY
    0x5555FF, // LIGHT_BLUE
    0x55FF55, // LIGHT_GREEN
    0x55FFFF, // LIGHT_CYAN
    0xFF5555, // LIGHT_RED
    0xFF55FF, // PINK
    0xFFFF55, // YELLOW
    0xFFFFFF  // WHITE
};

static uint32_t fg_color = 0xFFFFFF; // White
static uint32_t bg_color = 0x000000; // Black

// Track which character cells have text (for clearing text only)
static char text_buffer[80 * 25];

// Output redirection for piping
static char *redirect_buffer = NULL;
static int redirect_pos = 0;
static int redirect_max = 0;

void print_init(void)
{
    max_cols = get_screen_width() / CHAR_WIDTH;
    max_rows = get_screen_height() / CHAR_HEIGHT;
    cursor_x = 0;
    cursor_y = 0;

    // Clear text buffer
    for (int i = 0; i < 80 * 25; i++)
    {
        text_buffer[i] = ' ';
    }
}

void clear_row(size_t row)
{
    // Only clear the text area, not graphics
    for (size_t col = 0; col < max_cols; col++)
    {
        text_buffer[row * max_cols + col] = ' ';
        fill_rect(col * CHAR_WIDTH, row * CHAR_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
    }
}

void print_clear()
{
    // Clear all text but preserve graphics underneath
    for (size_t row = 0; row < max_rows; row++)
    {
        for (size_t col = 0; col < max_cols; col++)
        {
            text_buffer[row * max_cols + col] = ' ';
            fill_rect(col * CHAR_WIDTH, row * CHAR_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void scroll_screen(void)
{
    // Scroll text buffer up one line
    for (size_t row = 1; row < max_rows; row++)
    {
        for (size_t col = 0; col < max_cols; col++)
        {
            char c = text_buffer[row * max_cols + col];
            text_buffer[(row - 1) * max_cols + col] = c;

            // Redraw the character
            if (c != ' ')
            {
                draw_char(c, col * CHAR_WIDTH, (row - 1) * CHAR_HEIGHT, fg_color, bg_color);
            }
            else
            {
                fill_rect(col * CHAR_WIDTH, (row - 1) * CHAR_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
            }
        }
    }

    // Clear last row
    clear_row(max_rows - 1);
    cursor_y = max_rows - 1;
}

void print_newline()
{
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= max_rows)
    {
        scroll_screen();
    }
}

void print_set_cursor(size_t new_row, size_t new_col)
{
    if (new_row < max_rows && new_col < max_cols)
    {
        cursor_y = new_row;
        cursor_x = new_col;
    }
}

uint32_t print_get_cursor_x(void)
{
    return cursor_x;
}

uint32_t print_get_cursor_y(void)
{
    return cursor_y;
}

void print_move_cursor_left(void)
{
    if (cursor_x > 0)
    {
        cursor_x--;
    }
}

void print_move_cursor_right(void)
{
    if (cursor_x < max_cols - 1)
    {
        cursor_x++;
    }
}

// Enable output redirection to a buffer (for piping)
void print_redirect_to_buffer(char *buffer, int max_size)
{
    redirect_buffer = buffer;
    redirect_pos = 0;
    redirect_max = max_size;
    if (buffer)
    {
        buffer[0] = '\0';
    }
}

// Disable output redirection
void print_redirect_disable(void)
{
    redirect_buffer = NULL;
    redirect_pos = 0;
    redirect_max = 0;
}

// Get current redirect buffer position (length of captured output)
int print_redirect_get_length(void)
{
    return redirect_pos;
}

void print_char(char c)
{
    // If redirecting output, write to buffer instead of screen
    if (redirect_buffer)
    {
        if (redirect_pos < redirect_max - 1)
        {
            redirect_buffer[redirect_pos++] = c;
            redirect_buffer[redirect_pos] = '\0';
        }
        return;
    }

    if (c == '\n')
    {
        print_newline();
        return;
    }
    else if (c == '\b')
    {
        // Handle backspace
        if (cursor_x > 0)
        {
            cursor_x--;
        }
        else if (cursor_y > 0)
        {
            cursor_y--;
            cursor_x = max_cols - 1;
        }

        // Clear the character position
        text_buffer[cursor_y * max_cols + cursor_x] = ' ';
        fill_rect(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT,
                  CHAR_WIDTH, CHAR_HEIGHT, bg_color);
        return;
    }

    if (cursor_x >= max_cols)
    {
        print_newline();
    }

    // Store in text buffer
    text_buffer[cursor_y * max_cols + cursor_x] = c;

    // Draw the character using graphical font
    draw_char(c, cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, fg_color, bg_color);
    cursor_x++;
}

void print_str(char *str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
    {
        print_char(str[i]);
    }
}

void print_int(int num)
{
    char buffer[12];
    char *ptr = &buffer[11];
    *ptr = '\0';

    int is_negative = (num < 0);
    if (is_negative)
    {
        // Handle INT_MIN edge case
        if (num == -2147483648)
        {
            print_str("-2147483648");
            return;
        }
        num = -num;
    }

    do
    {
        *--ptr = (num % 10) + '0';
        num /= 10;
    } while (num);

    if (is_negative)
    {
        *--ptr = '-';
    }

    print_str(ptr);
}

void print_uint(uint32_t num)
{
    char buffer[12];
    char *ptr = &buffer[11];
    *ptr = '\0';

    do
    {
        *--ptr = (num % 10) + '0';
        num /= 10;
    } while (num);

    print_str(ptr);
}

void print_set_color(char foreground, char background)
{
    // Convert VGA color indices to RGB
    if (foreground < 16)
        fg_color = vga_to_rgb[foreground];
    if (background < 16)
        bg_color = vga_to_rgb[background];
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

void print_hex(uint64_t value)
{
    print_str("0x");
    char *hex_str = itoa((int)value, 16);
    print_str(hex_str);
}

void printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    for (size_t i = 0; format[i] != '\0'; i++)
    {
        if (format[i] == '%')
        {
            i++;

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
            {
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
            {
                const char *str = va_arg(args, const char *);
                int len = 0;
                while (str[len] != '\0')
                    len++;
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str((char *)str);
                break;
            }
            case 'd':
            {
                int num = va_arg(args, int);
                char *num_str = itoa(num, 10);
                int len = 0;
                while (num_str[len] != '\0')
                    len++;
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
            {
                int num = va_arg(args, int);
                char *hex_str = itoa(num, 16);
                int len = 0;
                while (hex_str[len] != '\0')
                    len++;
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str(hex_str);
                break;
            }
            case 'p':
            {
                uintptr_t ptr = va_arg(args, uintptr_t);
                print_str("0x");
                char *ptr_str = itoa(ptr, 16);
                int len = 0;
                while (ptr_str[len] != '\0')
                    len++;
                if (width > len)
                {
                    for (int pad = 0; pad < width - len; pad++)
                        print_char(' ');
                }
                print_str(ptr_str);
                break;
            }
            case '%':
                print_char('%');
                break;
            default:
                print_char('%');
                print_char(format[i]);
                break;
            }
        }
        else
        {
            print_char(format[i]);
        }
    }

    va_end(args);
}