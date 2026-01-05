#include <shell/shell.h>

#include <stdint.h>
#include <shell/print.h>
#include <interrupts/io/keyboard.h>
#include <graphics/graphics.h>
#include <interrupts/io/mouse.h>
#include <graphics/cursor.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <interrupts/port_io.h>
#include <memory/heap.h>

// Constants
#define MAX_COMMAND_LENGTH 256
#define MAX_COMMANDS 16
#define MAX_ARGS 16
#define PROMPT "> "

// Command buffer
static char command_buffer[MAX_COMMAND_LENGTH];
static uint16_t buffer_pos = 0;

// Argument parsing
static char *args[MAX_ARGS];
static int arg_count = 0;

// Command function type - now takes argc and argv
typedef void (*command_function)(int argc, char **argv);

// Command structure
typedef struct
{
    const char *name;
    const char *description;
    command_function function;
} command_t;

// Forward declarations for command functions
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_draw(int argc, char **argv);
static void cmd_cls(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_heap(int argc, char **argv);
static void cmd_touch(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_write(int argc, char **argv);
static void cmd_echo(int argc, char **argv);

// Command table
static const command_t commands[MAX_COMMANDS] = {
    {"help", "Display available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {"draw", "Draw shapes (draw triangle|rect|line|pixel)", cmd_draw},
    {"cls", "Clear the graphics screen", cmd_cls},
    {"ls", "List directory contents", cmd_ls},
    {"cat", "Display file contents", cmd_cat},
    {"heap", "Show heap statistics", cmd_heap},
    {"touch", "Create a new file", cmd_touch},
    {"rm", "Remove a file", cmd_rm},
    {"write", "Write text to a file", cmd_write},
    {"echo", "Print text to the screen", cmd_echo},
    {NULL, NULL, NULL} // Sentinel
};

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

// Parse command line into arguments
static void parse_arguments(void)
{
    arg_count = 0;
    char *ptr = command_buffer;

    // Skip leading spaces
    while (*ptr == ' ')
        ptr++;

    while (*ptr && arg_count < MAX_ARGS)
    {
        // Start of argument
        args[arg_count++] = ptr;

        // Find end of argument (space or null)
        while (*ptr && *ptr != ' ')
            ptr++;

        // Null-terminate this argument
        if (*ptr)
        {
            *ptr = '\0';
            ptr++;
            // Skip spaces between arguments
            while (*ptr == ' ')
                ptr++;
        }
    }
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

    // Parse into arguments
    parse_arguments();

    if (arg_count == 0)
    {
        return;
    }

    // First argument is the command name
    for (int i = 0; i < MAX_COMMANDS; i++)
    {
        if (commands[i].name == NULL)
        {
            break;
        }

        if (strcmp(args[0], commands[i].name) == 0)
        {
            commands[i].function(arg_count, args);
            return;
        }
    }

    // Command not found
    print_str("Unknown command: ");
    print_str(args[0]);
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

// Helper to parse color
static uint32_t parse_color(const char *str)
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

static void cmd_cls(int argc, char **argv)
{
    clear_screen(COLOR_BLACK);
    print_str("Graphics screen cleared\n");
}

static void cmd_heap(int argc, char **argv)
{
    uint32_t total, used, free_mem;
    heap_stats(&total, &used, &free_mem);

    if (total == 0)
    {
        print_str("Heap not initialized\n");
        return;
    }

    print_str("Heap Statistics:\n");
    print_str("  Total: ");
    print_uint(total / 1024);
    print_str(" KB\n");

    print_str("  Used:  ");
    print_uint(used / 1024);
    print_str(" KB (");
    print_uint((used * 100) / total);
    print_str("%)\n");

    print_str("  Free:  ");
    print_uint(free_mem / 1024);
    print_str(" KB (");
    print_uint((free_mem * 100) / total);
    print_str("%)\n");
}

static void cmd_draw(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: draw <shape> [args]\n");
        print_str("Shapes:\n");
        print_str("  triangle <x0> <y0> <x1> <y1> <x2> <y2> <color> <filled>\n");
        print_str("  rect <x> <y> <w> <h> <color>\n");
        print_str("  line <x0> <y0> <x1> <y1> <color>\n");
        print_str("  pixel <x> <y> <color>\n");
        print_str("  circle <cx> <cy> <radius> <color> [filled]\n");
        print_str("Colors: red, green, blue, white, yellow, cyan, magenta\n");
        return;
    }

    if (strcmp(argv[1], "triangle") == 0)
    {
        if (argc < 10)
        {
            print_str("Usage: draw triangle <x0> <y0> <x1> <y1> <x2> <y2> <color> <filled>\n");
            return;
        }

        int x0 = atoi(argv[2]);
        int y0 = atoi(argv[3]);
        int x1 = atoi(argv[4]);
        int y1 = atoi(argv[5]);
        int x2 = atoi(argv[6]);
        int y2 = atoi(argv[7]);
        uint32_t color = parse_color(argv[8]);
        int isFilled = 0;

        if (argc >= 10 && strcmp(argv[9], "filled") == 0)
        {
            isFilled = 1;
        }

        draw_triangle(x0, y0, x1, y1, x2, y2, color, isFilled);
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
    else if (strcmp(argv[1], "circle") == 0)
    {
        if (argc < 6)
        {
            print_str("Usage: draw circle <cx> <cy> <radius> <color> [filled]\n");
            return;
        }

        int cx = atoi(argv[2]);
        int cy = atoi(argv[3]);
        int radius = atoi(argv[4]);
        uint32_t color = parse_color(argv[5]);
        int isFilled = 0;

        if (argc >= 7 && strcmp(argv[6], "filled") == 0)
        {
            isFilled = 1;
        }

        draw_circle(cx, cy, radius, color, isFilled);
        print_str("Circle drawn!\n");
    }
    else
    {
        print_str("Unknown shape: ");
        print_str(argv[1]);
        print_str("\n");
    }
}

// Command implementations - all now take argc/argv
static void cmd_help(int argc, char **argv)
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

static void cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/";

    vfs_node_t *dir = vfs_resolve_path(path);
    if (!dir)
    {
        print_str("Directory not found\n");
        return;
    }

    if (!(dir->flags & VFS_DIRECTORY))
    {
        print_str("Not a directory\n");
        return;
    }

    print_str("Contents of ");
    print_str(path);
    print_str(":\n");

    if (!dir->readdir)
    {
        print_str("Error: Directory listing not supported\n");
        return;
    }

    uint32_t i = 0;
    vfs_node_t *child;
    int file_count = 0;

    while (i < 64 && (child = vfs_readdir(dir, i)) != NULL)
    {
        print_str("  ");
        if (child->flags & VFS_DIRECTORY)
            print_str("[DIR]  ");
        else
            print_str("[FILE] ");
        print_str(child->name);
        print_str("\n");

        kfree(child);
        file_count++;
        i++;
    }

    if (file_count == 0)
    {
        print_str("  (empty)\n");
    }
    else
    {
        print_str("\nTotal: ");
        print_int(file_count);
        print_str(" item(s)\n");
    }
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: cat <file>\n");
        return;
    }

    vfs_node_t *file = vfs_open(argv[1], VFS_READ);
    if (!file)
    {
        print_str("File not found\n");
        return;
    }

    uint8_t buffer[256];
    int bytes = vfs_read(file, 0, 256, buffer);

    if (bytes > 0)
    {
        for (int i = 0; i < bytes; i++)
        {
            print_char(buffer[i]);
        }
    }

    vfs_close(file);
}

static void cmd_clear(int argc, char **argv)
{
    print_clear();
}

static void cmd_info(int argc, char **argv)
{
    print_str("Simple OS Shell\n");
    print_str("Version 0.1\n");
    print_str("System is running!\n");
}

static void cmd_reboot(int argc, char **argv)
{
    print_str("Rebooting system...\n");
    uint8_t good = 0x02;
    while (good & 0x02)
    {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

static void cmd_touch(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: touch <filename>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root)
    {
        print_str("Error: No filesystem mounted\n");
        return;
    }

    // Check if file already exists
    vfs_node_t *existing = vfs_finddir(root, argv[1]);
    if (existing)
    {
        print_str("File already exists: ");
        print_str(argv[1]);
        print_str("\n");
        kfree(existing);
        return;
    }

    // Create the file
    if (root->create)
    {
        int result = root->create(root, argv[1], VFS_FILE);
        if (result == 0)
        {
            print_str("Created: ");
            print_str(argv[1]);
            print_str("\n");
        }
        else
        {
            print_str("Error creating file\n");
        }
    }
    else
    {
        print_str("Error: Filesystem does not support file creation\n");
    }
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: rm <filename>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root)
    {
        print_str("Error: No filesystem mounted\n");
        return;
    }

    // Check if file exists
    vfs_node_t *existing = vfs_finddir(root, argv[1]);
    if (!existing)
    {
        print_str("File not found: ");
        print_str(argv[1]);
        print_str("\n");
        return;
    }
    kfree(existing);

    // Delete the file
    if (root->delete)
    {
        int result = root->delete(root, argv[1]);
        if (result == 0)
        {
            print_str("Removed: ");
            print_str(argv[1]);
            print_str("\n");
        }
        else
        {
            print_str("Error removing file\n");
        }
    }
    else
    {
        print_str("Error: Filesystem does not support file deletion\n");
    }
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 3)
    {
        print_str("Usage: write <filename> <text...>\n");
        return;
    }

    vfs_node_t *file = vfs_open(argv[1], VFS_WRITE);
    if (!file)
    {
        print_str("File not found: ");
        print_str(argv[1]);
        print_str("\nTip: Use 'touch' to create a file first\n");
        return;
    }

    // Concatenate all remaining arguments as the content
    char content[512];
    int pos = 0;
    for (int i = 2; i < argc && pos < 500; i++)
    {
        char *arg = argv[i];
        while (*arg && pos < 500)
        {
            content[pos++] = *arg++;
        }
        if (i < argc - 1 && pos < 500)
        {
            content[pos++] = ' ';
        }
    }
    content[pos] = '\0';

    int bytes = vfs_write(file, 0, pos, (uint8_t *)content);
    vfs_close(file);

    if (bytes > 0)
    {
        print_str("Wrote ");
        print_int(bytes);
        print_str(" bytes to ");
        print_str(argv[1]);
        print_str("\n");
    }
    else
    {
        print_str("Error writing to file\n");
    }
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        print_str(argv[i]);
        if (i < argc - 1)
        {
            print_str(" ");
        }
    }
    print_str("\n");
}

// Initialize the shell
void shell_init(void)
{
    reset_command_buffer();
    clear_screen(COLOR_BLACK);
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

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putchar(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, c);
}

void serial_print(const char *str)
{
    while (*str)
    {
        serial_putchar(*str++);
    }
}

void serial_print_hex(uint64_t value)
{
    char hex[] = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4)
    {
        serial_putchar(hex[(value >> i) & 0xF]);
    }
}

void serial_print_dec(uint32_t value)
{
    char buffer[32];
    int i = 0;

    if (value == 0)
    {
        serial_putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        serial_putchar(buffer[--i]);
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
            shell_process_char(c);
            if (c >= 32 && c != 127) // printable ASCII only
            {
                print_char(c);
            }
        }

        static int32_t last_x = -1;
        static int32_t last_y = -1;
        static uint8_t last_buttons = 0;

        // Handle mouse
        mouse_state_t mouse = mouse_get_state();

        // Update if position OR buttons changed
        if (mouse.x != last_x || mouse.y != last_y || mouse.buttons != last_buttons)
        {
            cursor_update(mouse.x, mouse.y);
            last_x = mouse.x;
            last_y = mouse.y;
            last_buttons = mouse.buttons;

            // Optional: Handle click events
            if (mouse.buttons & MOUSE_LEFT_BUTTON)
            {
                set_cursor_color(0xFF0000); // Change cursor color on click

                draw_circle(mouse.x, mouse.y, 3, 0xFFFF00, 1);
                // You can add click handling here
                // For example, draw a dot where clicked:
                // put_pixel(mouse.x, mouse.y, 0xFFFF00);
            }
            else if (mouse.buttons & MOUSE_RIGHT_BUTTON)
            {
                set_cursor_color(0x0000FF); // Change cursor color on right click
                // clear the dot where right-clicked:
                draw_circle(mouse.x, mouse.y, 3, COLOR_BLACK, 1);
            }
            else if (mouse.buttons & MOUSE_MIDDLE_BUTTON)
            {
                set_cursor_color(0x00FF00); // Change cursor color on middle click
            }
            else
            {
                set_cursor_color(0xFFFFFF); // Default cursor color
            }
        }

        // // Debug: show interrupt count occasionally
        // extern volatile uint32_t mouse_interrupt_count;
        // static uint32_t last_count = 0;
        // static int tick = 0;
        // if (++tick > 100000)
        // {
        //     tick = 0;
        //     if (mouse_interrupt_count != last_count)
        //     {
        //         serial_print("IRQ count: ");
        //         serial_print_dec(mouse_interrupt_count);
        //         serial_print("\n");
        //         last_count = mouse_interrupt_count;
        //     }
        // }
    }
}