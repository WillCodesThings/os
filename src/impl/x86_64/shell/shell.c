#include <stdint.h>
#include <shell/shell.h>
#include <shell/print.h>
#include <interrupts/io/keyboard.h>

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

// Command table
static const command_t commands[MAX_COMMANDS] = {
    {"help", "Display available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {NULL, NULL, NULL}, // End of commands
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
            shell_process_char(c);
            print_char(c); // Echo the character
        }
    }
}