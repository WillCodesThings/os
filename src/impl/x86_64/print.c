#include <print.h>

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

void print_char(char c)
{
    if (c == '\n')
    {
        print_newline();
        return;
    }

    if (col > NUM_COLS)
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