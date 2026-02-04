// Shell - core shell functionality
// Commands are implemented in commands/*.c files

#include <shell/shell.h>
#include <shell/commands.h>
#include <stdint.h>
#include <shell/print.h>
#include <interrupts/io/keyboard.h>
#include <graphics/graphics.h>
#include <interrupts/io/mouse.h>
#include <graphics/cursor.h>
#include <fs/vfs.h>
#include <memory/heap.h>
#include <gui/window.h>
#include <gui/desktop.h>
#include <net/net.h>

// Constants
#define MAX_COMMAND_LENGTH 256
#define MAX_ARGS 16
#define PROMPT "> "
#define HISTORY_SIZE 32

// Command buffer
static char command_buffer[MAX_COMMAND_LENGTH];
static uint16_t buffer_pos = 0;
static uint16_t cursor_pos = 0;

// Command history
static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;
static int history_pos = -1;
static char saved_buffer[MAX_COMMAND_LENGTH];
static uint16_t saved_buffer_pos = 0;

// Argument parsing
static char *args[MAX_ARGS];
static int arg_count = 0;

// Track prompt position for line redraw
static uint32_t prompt_row = 0;
static uint32_t prompt_col = 0;

// Piping support
#define PIPE_BUFFER_SIZE 4096
static char *pipe_stdin = NULL;
static int pipe_stdin_len = 0;

// ============================================================================
// String utilities
// ============================================================================

static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

static uint16_t strlen(const char *s)
{
    uint16_t len = 0;
    while (s[len])
        len++;
    return len;
}

static void strcpy(char *dest, const char *src)
{
    while (*src)
        *dest++ = *src++;
    *dest = '\0';
}

static int strncmp(const char *s1, const char *s2, uint16_t n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return *s1 - *s2;
}

static void strncpy(char *dest, const char *src, uint16_t n)
{
    while (n && *src)
    {
        *dest++ = *src++;
        n--;
    }
    *dest = '\0';
}

// ============================================================================
// Shell utility functions (exported for commands)
// ============================================================================

const char *shell_get_stdin(void)
{
    return pipe_stdin;
}

int shell_get_stdin_len(void)
{
    return pipe_stdin_len;
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

// ============================================================================
// Line editing and history
// ============================================================================

static void redraw_line(void)
{
    print_set_cursor(prompt_row, prompt_col);
    for (uint16_t i = 0; i < buffer_pos; i++)
        print_char(command_buffer[i]);
    print_str("    "); // Clear trailing chars
    print_set_cursor(prompt_row, prompt_col + cursor_pos);
}

static void add_to_history(void)
{
    if (buffer_pos == 0)
        return;
    if (history_count > 0 && strcmp(command_buffer, history[history_count - 1]) == 0)
        return;

    if (history_count >= HISTORY_SIZE)
    {
        for (int i = 0; i < HISTORY_SIZE - 1; i++)
            strcpy(history[i], history[i + 1]);
        history_count = HISTORY_SIZE - 1;
    }

    strcpy(history[history_count++], command_buffer);
}

static void load_history_entry(int index)
{
    if (index < 0)
    {
        strcpy(command_buffer, saved_buffer);
        buffer_pos = saved_buffer_pos;
    }
    else if (index < history_count)
    {
        strcpy(command_buffer, history[index]);
        buffer_pos = strlen(command_buffer);
    }

    cursor_pos = buffer_pos;
    redraw_line();
}

static void reset_command_buffer(void)
{
    buffer_pos = 0;
    cursor_pos = 0;
    history_pos = -1;
    for (int i = 0; i < MAX_COMMAND_LENGTH; i++)
        command_buffer[i] = 0;
}

// ============================================================================
// Command parsing and execution
// ============================================================================

static void parse_arguments(void)
{
    arg_count = 0;
    char *ptr = command_buffer;

    while (*ptr == ' ')
        ptr++;

    while (*ptr && arg_count < MAX_ARGS)
    {
        args[arg_count++] = ptr;
        while (*ptr && *ptr != ' ')
            ptr++;
        if (*ptr)
        {
            *ptr = '\0';
            ptr++;
            while (*ptr == ' ')
                ptr++;
        }
    }
}

static char *find_pipe(char *cmd)
{
    while (*cmd)
    {
        if (*cmd == '|')
            return cmd;
        cmd++;
    }
    return 0;
}

static void execute_single_command(char *cmd_str)
{
    while (*cmd_str == ' ')
        cmd_str++;

    int len = strlen(cmd_str);
    while (len > 0 && cmd_str[len - 1] == ' ')
    {
        cmd_str[len - 1] = '\0';
        len--;
    }

    if (len == 0)
        return;

    static char *cmd_args[MAX_ARGS];
    int cmd_arg_count = 0;
    char *ptr = cmd_str;

    while (*ptr && cmd_arg_count < MAX_ARGS)
    {
        cmd_args[cmd_arg_count++] = ptr;
        while (*ptr && *ptr != ' ')
            ptr++;
        if (*ptr)
        {
            *ptr = '\0';
            ptr++;
            while (*ptr == ' ')
                ptr++;
        }
    }

    if (cmd_arg_count == 0)
        return;

    const command_t *commands = commands_get_table();
    for (int i = 0; commands[i].name; i++)
    {
        if (strcmp(cmd_args[0], commands[i].name) == 0)
        {
            commands[i].function(cmd_arg_count, cmd_args);
            return;
        }
    }

    print_str("Unknown command: ");
    print_str(cmd_args[0]);
    print_str("\n");
}

static void execute_piped_command(void)
{
    char *pipe_pos = find_pipe(command_buffer);
    if (!pipe_pos)
    {
        pipe_stdin = 0;
        pipe_stdin_len = 0;
        execute_single_command(command_buffer);
        return;
    }

    char *output_buffer = kmalloc(PIPE_BUFFER_SIZE);
    if (!output_buffer)
    {
        print_str("Error: Out of memory for pipe\n");
        return;
    }

    *pipe_pos = '\0';
    char *cmd1 = command_buffer;
    char *cmd2 = pipe_pos + 1;

    pipe_stdin = 0;
    pipe_stdin_len = 0;
    print_redirect_to_buffer(output_buffer, PIPE_BUFFER_SIZE);
    execute_single_command(cmd1);
    print_redirect_disable();

    pipe_stdin = output_buffer;
    pipe_stdin_len = print_redirect_get_length();

    char *next_pipe = find_pipe(cmd2);
    if (next_pipe)
    {
        *next_pipe = '\0';
        char *cmd3 = next_pipe + 1;

        char *output_buffer2 = kmalloc(PIPE_BUFFER_SIZE);
        if (output_buffer2)
        {
            print_redirect_to_buffer(output_buffer2, PIPE_BUFFER_SIZE);
            execute_single_command(cmd2);
            print_redirect_disable();

            kfree(output_buffer);
            pipe_stdin = output_buffer2;
            pipe_stdin_len = print_redirect_get_length();
            output_buffer = output_buffer2;

            execute_single_command(cmd3);
        }
    }
    else
    {
        execute_single_command(cmd2);
    }

    pipe_stdin = 0;
    pipe_stdin_len = 0;
    kfree(output_buffer);
}

static void execute_command(void)
{
    if (buffer_pos == 0)
        return;

    command_buffer[buffer_pos] = '\0';
    add_to_history();

    if (find_pipe(command_buffer))
    {
        execute_piped_command();
        return;
    }

    pipe_stdin = 0;
    pipe_stdin_len = 0;
    parse_arguments();

    if (arg_count == 0)
        return;

    const command_t *commands = commands_get_table();
    for (int i = 0; commands[i].name; i++)
    {
        if (strcmp(args[0], commands[i].name) == 0)
        {
            commands[i].function(arg_count, args);
            return;
        }
    }

    print_str("Unknown command: ");
    print_str(args[0]);
    print_str("\nType 'help' for available commands\n");
}

// ============================================================================
// Tab completion
// ============================================================================

#define MAX_COMPLETIONS 16
#define MAX_COMPLETION_LEN 64

static uint16_t find_common_prefix(char matches[][MAX_COMPLETION_LEN], int count, uint16_t prefix_len)
{
    if (count == 0)
        return prefix_len;
    if (count == 1)
        return strlen(matches[0]);

    uint16_t common_len = strlen(matches[0]);
    for (int i = 1; i < count; i++)
    {
        uint16_t j = prefix_len;
        while (j < common_len && matches[0][j] == matches[i][j])
            j++;
        common_len = j;
    }
    return common_len;
}

static void handle_tab_completion(void)
{
    int word_start = cursor_pos;
    while (word_start > 0 && command_buffer[word_start - 1] != ' ')
        word_start--;

    int completing_command = 1;
    for (int i = 0; i < word_start; i++)
    {
        if (command_buffer[i] != ' ')
        {
            completing_command = 0;
            break;
        }
    }

    char prefix[MAX_COMPLETION_LEN];
    uint16_t prefix_len = cursor_pos - word_start;
    if (prefix_len >= MAX_COMPLETION_LEN)
        prefix_len = MAX_COMPLETION_LEN - 1;
    strncpy(prefix, command_buffer + word_start, prefix_len);
    prefix[prefix_len] = '\0';

    char matches[MAX_COMPLETIONS][MAX_COMPLETION_LEN];
    int match_count = 0;

    if (completing_command)
    {
        const command_t *commands = commands_get_table();
        for (int i = 0; commands[i].name && match_count < MAX_COMPLETIONS; i++)
        {
            if (strncmp(commands[i].name, prefix, prefix_len) == 0)
            {
                strncpy(matches[match_count], commands[i].name, MAX_COMPLETION_LEN - 1);
                matches[match_count][MAX_COMPLETION_LEN - 1] = '\0';
                match_count++;
            }
        }
    }
    else
    {
        vfs_node_t *root = vfs_get_root();
        if (root && root->readdir)
        {
            uint32_t idx = 0;
            vfs_node_t *child;
            while ((child = vfs_readdir(root, idx++)) && match_count < MAX_COMPLETIONS)
            {
                if (strncmp(child->name, prefix, prefix_len) == 0)
                {
                    strncpy(matches[match_count], child->name, MAX_COMPLETION_LEN - 1);
                    matches[match_count][MAX_COMPLETION_LEN - 1] = '\0';
                    match_count++;
                }
                kfree(child);
            }
        }
    }

    if (match_count == 0)
        return;

    if (match_count == 1)
    {
        uint16_t complete_len = strlen(matches[0]);
        uint16_t insert_len = complete_len - prefix_len;

        if (buffer_pos + insert_len + 1 < MAX_COMMAND_LENGTH)
        {
            for (int i = buffer_pos; i >= (int)cursor_pos; i--)
                command_buffer[i + insert_len + 1] = command_buffer[i];

            for (uint16_t i = 0; i < insert_len; i++)
                command_buffer[cursor_pos + i] = matches[0][prefix_len + i];
            command_buffer[cursor_pos + insert_len] = ' ';

            buffer_pos += insert_len + 1;
            cursor_pos += insert_len + 1;
            command_buffer[buffer_pos] = '\0';
            redraw_line();
        }
    }
    else
    {
        uint16_t common_len = find_common_prefix(matches, match_count, prefix_len);

        if (common_len > prefix_len)
        {
            uint16_t insert_len = common_len - prefix_len;
            if (buffer_pos + insert_len < MAX_COMMAND_LENGTH)
            {
                for (int i = buffer_pos; i >= (int)cursor_pos; i--)
                    command_buffer[i + insert_len] = command_buffer[i];

                for (uint16_t i = 0; i < insert_len; i++)
                    command_buffer[cursor_pos + i] = matches[0][prefix_len + i];

                buffer_pos += insert_len;
                cursor_pos += insert_len;
                command_buffer[buffer_pos] = '\0';
                redraw_line();
            }
        }
        else
        {
            print_str("\n");
            for (int i = 0; i < match_count; i++)
            {
                print_str(matches[i]);
                print_str("  ");
            }
            print_str("\n");
            print_str(PROMPT);
            prompt_row = print_get_cursor_y();
            prompt_col = print_get_cursor_x();
            for (uint16_t i = 0; i < buffer_pos; i++)
                print_char(command_buffer[i]);
            print_set_cursor(prompt_row, prompt_col + cursor_pos);
        }
    }
}

// ============================================================================
// Input processing
// ============================================================================

void shell_process_char(int c)
{
    if (c == '\n')
    {
        print_str("\n");
        execute_command();
        reset_command_buffer();
        print_str(PROMPT);
        prompt_row = print_get_cursor_y();
        prompt_col = print_get_cursor_x();
    }
    else if (c == '\b')
    {
        if (cursor_pos > 0)
        {
            for (uint16_t i = cursor_pos - 1; i < buffer_pos - 1; i++)
                command_buffer[i] = command_buffer[i + 1];
            buffer_pos--;
            cursor_pos--;
            command_buffer[buffer_pos] = '\0';
            redraw_line();
        }
    }
    else if (c == KEY_DELETE)
    {
        if (cursor_pos < buffer_pos)
        {
            for (uint16_t i = cursor_pos; i < buffer_pos - 1; i++)
                command_buffer[i] = command_buffer[i + 1];
            buffer_pos--;
            command_buffer[buffer_pos] = '\0';
            redraw_line();
        }
    }
    else if (c == KEY_LEFT)
    {
        if (cursor_pos > 0)
        {
            cursor_pos--;
            print_move_cursor_left();
        }
    }
    else if (c == KEY_RIGHT)
    {
        if (cursor_pos < buffer_pos)
        {
            cursor_pos++;
            print_move_cursor_right();
        }
    }
    else if (c == KEY_HOME)
    {
        cursor_pos = 0;
        print_set_cursor(prompt_row, prompt_col);
    }
    else if (c == KEY_END)
    {
        cursor_pos = buffer_pos;
        print_set_cursor(prompt_row, prompt_col + buffer_pos);
    }
    else if (c == KEY_UP)
    {
        if (history_count > 0)
        {
            if (history_pos == -1)
            {
                strcpy(saved_buffer, command_buffer);
                saved_buffer_pos = buffer_pos;
                history_pos = history_count - 1;
            }
            else if (history_pos > 0)
            {
                history_pos--;
            }
            load_history_entry(history_pos);
        }
    }
    else if (c == KEY_DOWN)
    {
        if (history_pos >= 0)
        {
            if (history_pos < history_count - 1)
            {
                history_pos++;
                load_history_entry(history_pos);
            }
            else
            {
                history_pos = -1;
                load_history_entry(-1);
            }
        }
    }
    else if (c == '\t')
    {
        handle_tab_completion();
    }
    else if (c >= 32 && c < 127)
    {
        if (buffer_pos < MAX_COMMAND_LENGTH - 1)
        {
            for (uint16_t i = buffer_pos; i > cursor_pos; i--)
                command_buffer[i] = command_buffer[i - 1];
            command_buffer[cursor_pos] = c;
            buffer_pos++;
            cursor_pos++;
            command_buffer[buffer_pos] = '\0';
            redraw_line();
        }
    }
}

// ============================================================================
// Shell initialization and main loop
// ============================================================================

void shell_init(void)
{
    reset_command_buffer();
    clear_screen(COLOR_BLACK);
    print_str("Shell initialized\n");
    print_str("Use arrow keys to edit, up/down for history, tab to complete\n");
}

void shell_run(void)
{
    shell_init();
    print_str(PROMPT);
    prompt_row = print_get_cursor_y();
    prompt_col = print_get_cursor_x();

    while (1)
    {
        if (keyboard_available())
        {
            int c = keyboard_read();

            int handled = 0;
            if (desktop_is_active())
                handled = desktop_handle_key(c);

            if (!handled)
                shell_process_char(c);
        }

        static int32_t last_x = -1;
        static int32_t last_y = -1;
        static uint8_t last_buttons = 0;

        mouse_state_t mouse = mouse_get_state();

        if (mouse.x != last_x || mouse.y != last_y || mouse.buttons != last_buttons)
        {
            int desktop_handled = 0;
            if (desktop_is_active())
                desktop_handled = desktop_handle_mouse(mouse.x, mouse.y, mouse.buttons);

            if (!desktop_handled)
                wm_handle_mouse(mouse.x, mouse.y, mouse.buttons);

            if (wm_is_dragging())
            {
                set_cursor_state(CURSOR_MOVE);
                set_cursor_color(0xFFFFFF);
            }
            else
            {
                window_t *win = wm_get_window_at(mouse.x, mouse.y);
                if (win && mouse.y < win->y + WINDOW_TITLE_HEIGHT && mouse.y >= win->y)
                    set_cursor_state(CURSOR_HAND);
                else
                    set_cursor_state(CURSOR_ARROW);
                set_cursor_color(0xFFFFFF);
            }

            cursor_update(mouse.x, mouse.y);
            last_x = mouse.x;
            last_y = mouse.y;
            last_buttons = mouse.buttons;
        }

        wm_render();
        net_process_packet();

        // Halt CPU until next interrupt to reduce power consumption
        asm volatile("hlt");
    }
}

// ============================================================================
// Serial debug utilities (used by some parts of the kernel)
// ============================================================================

#define COM1 0x3F8

void serial_init(void)
{
    extern void outb(uint16_t port, uint8_t value);
    extern uint8_t inb(uint16_t port);

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
    extern void outb(uint16_t port, uint8_t value);
    extern uint8_t inb(uint16_t port);

    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, c);
}

void serial_print(const char *str)
{
    while (*str)
        serial_putchar(*str++);
}

void serial_print_hex(uint64_t value)
{
    char hex[] = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4)
        serial_putchar(hex[(value >> i) & 0xF]);
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
        serial_putchar(buffer[--i]);
}
