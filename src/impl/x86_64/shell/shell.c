#include <stdint.h>
#include <shell/shell.h>
#include <shell/print.h>
#include <interrupts/io/keyboard.h>
#include <graphics/graphics.h>

// Constants
#define MAX_COMMAND_LENGTH 64
#define MAX_COMMANDS 8
#define PROMPT "> "

// Command buffer
static char command_buffer[MAX_COMMAND_LENGTH];
static uint8_t buffer_pos = 0;

// Command function type
typedef void (*command_function)(void);

// Command structure
typedef struct
{
    const char *name;
    const char *description;
    command_function function;
} command_t;

// Forward declarations for command functions
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_info(void);
static void cmd_reboot(void);
static void cmd_draw(int argc, char **argv);
static void cmd_cls(void);

// Command table
static const command_t commands[MAX_COMMANDS] = {
    {"help", "Display available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {"draw", "Draw shapes (draw triangle|rect|line|pixel)", cmd_draw},
    {NULL, NULL, NULL},
    {NULL, NULL, NULL},
    {NULL, NULL, NULL}};

// String comparison function
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

// String length function
static uint8_t strlen(const char *s)
{
    uint8_t len = 0;
    while (s[len])
    {
        len++;
    }
    return len;
}

// Execute command
static void execute_command(void)
{
    // Empty command
    if (buffer_pos == 0)
    {
        return;
    }

    // Null-terminate the command
    command_buffer[buffer_pos] = '\0';

    // Check for built-in commands
    for (int i = 0; i < MAX_COMMANDS && commands[i].name != NULL; i++)
    {
        if (strcmp(command_buffer, commands[i].name) == 0)
        {
            commands[i].function();
            return;
        }
    }

    // Command not found
    print_str("Unknown command: ");
    print_str(command_buffer);
    print_str("\nType 'help' for available commands\n");
}

// Helper function to reset command buffer
static void reset_command_buffer(void)
{
    buffer_pos = 0;
    for (int i = 0; i < MAX_COMMAND_LENGTH; i++)
    {
        command_buffer[i] = 0;
    }
}

static void cmd_cls(void)
{
    clear_screen(COLOR_BLACK);
    print_str("Graphics screen cleared\n");
}

static void cmd_draw(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: draw <shape> [args]\n");
        print_str("Shapes:\n");
        print_str("  triangle <x0> <y0> <x1> <y1> <x2> <y2> <color>\n");
        print_str("  rect <x> <y> <w> <h> <color>\n");
        print_str("  line <x0> <y0> <x1> <y1> <color>\n");
        print_str("  pixel <x> <y> <color>\n");
        print_str("Colors: red, green, blue, white, yellow, cyan, magenta\n");
        return;
    }

    // Helper to parse color
    uint32_t parse_color(const char *str)
    {
        if (strcmp(str, "red") == 0)
            return COLOR_RED;
        if (strcmp(str, "green") == 0)
            return COLOR_GREEN;
        if (strcmp(str, "blue") == 0)
            return COLOR_BLUE;
        if (strcmp(str, "white") == 0)
            return COLOR_WHITE;
        if (strcmp(str, "yellow") == 0)
            return COLOR_YELLOW;
        if (strcmp(str, "cyan") == 0)
            return COLOR_CYAN;
        if (strcmp(str, "magenta") == 0)
            return COLOR_MAGENTA;
        if (strcmp(str, "black") == 0)
            return COLOR_BLACK;
        return COLOR_WHITE; // default
    }

    if (strcmp(argv[1], "triangle") == 0)
    {
        if (argc < 9)
        {
            print_str("Usage: draw triangle <x0> <y0> <x1> <y1> <x2> <y2> <color>\n");
            return;
        }

        int x0 = atoi(argv[2]);
        int y0 = atoi(argv[3]);
        int x1 = atoi(argv[4]);
        int y1 = atoi(argv[5]);
        int x2 = atoi(argv[6]);
        int y2 = atoi(argv[7]);
        uint32_t color = parse_color(argv[8]);

        draw_triangle(x0, y0, x1, y1, x2, y2, color);
        print_str("Triangle drawn!\n");
    }
    else if (strcmp(argv[1], "rect") == 0)
    {
        if (argc < 7)
        {
            print_str("Usage: draw rect <x> <y> <w> <h> <color>\n");
            return;
        }

        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        int w = atoi(argv[4]);
        int h = atoi(argv[5]);
        uint32_t color = parse_color(argv[6]);

        fill_rect(x, y, w, h, color);
        print_str("Rectangle drawn!\n");
    }
    else if (strcmp(argv[1], "line") == 0)
    {
        if (argc < 7)
        {
            print_str("Usage: draw line <x0> <y0> <x1> <y1> <color>\n");
            return;
        }

        int x0 = atoi(argv[2]);
        int y0 = atoi(argv[3]);
        int x1 = atoi(argv[4]);
        int y1 = atoi(argv[5]);
        uint32_t color = parse_color(argv[6]);

        draw_line(x0, y0, x1, y1, color);
        print_str("Line drawn!\n");
    }
    else if (strcmp(argv[1], "pixel") == 0)
    {
        if (argc < 5)
        {
            print_str("Usage: draw pixel <x> <y> <color>\n");
            return;
        }

        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        uint32_t color = parse_color(argv[4]);

        put_pixel(x, y, color);
        print_str("Pixel drawn!\n");
    }
    else
    {
        print_str("Unknown shape: ");
        print_str(argv[1]);
        print_str("\n");
    }
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    if (*str == '-')
    {
        sign = -1;
        str++;
    }

    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

// Command implementations
static void cmd_help(void)
{
    print_str("Available commands:\n");
    for (int i = 0; i < MAX_COMMANDS && commands[i].name != NULL; i++)
    {
        print_str("  ");
        print_str(commands[i].name);
        print_str(" - ");
        print_str(commands[i].description);
        print_str("\n");
    }
}

static void cmd_clear(void)
{
    // Clear screen - simple implementation
    for (int i = 0; i < 25; i++)
    {
        print_str("\n");
    }
}

static void cmd_info(void)
{
    print_str("Simple OS Shell\n");
    print_str("Version 0.1\n");
    print_str("System is running!\n");
}

static void cmd_reboot(void)
{
    print_str("Rebooting system...\n");
    // Note: This is a simplified implementation
    // In a real system, you would use proper reboot mechanisms
    // This would typically involve writing to I/O ports
    uint8_t good = 0x02;
    while (good & 0x02)
    {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

// Initialize the shell
void shell_init(void)
{
    reset_command_buffer();
    print_str("Shell initialized\n");
}

// Process a single character of input
void shell_process_char(char c)
{
    if (c == '\n')
    {
        // Execute the command on Enter
        print_str("\n");
        execute_command();
        reset_command_buffer();
        print_str(PROMPT);
    }
    else if (c == '\b')
    {
        // Handle backspace
        if (buffer_pos > 0)
        {
            buffer_pos--;
            // Move cursor back, print space, move cursor back again
            print_str("\b \b");
        }
    }
    else if (buffer_pos < MAX_COMMAND_LENGTH - 1)
    {
        // Add character to buffer
        command_buffer[buffer_pos++] = c;
    }
}

// Main shell loop
void shell_run(void)
{
    shell_init();
    print_str(PROMPT);

    while (1)
    {
        if (keyboard_available())
        {
            char c = keyboard_read();
            if (c == "\b")
            {
            }
            shell_process_char(c);
            if (c >= 32 && c != 127) // printable ASCII only
            {
                print_char(c);
            }
        }
    }
}