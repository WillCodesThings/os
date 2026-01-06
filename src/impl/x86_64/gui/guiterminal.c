#include "gui/guiterminal.h"
#include "gui/window.h"
#include "gui/imageviewer.h"
#include "gui/desktop.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "graphics/font.h"
#include "fs/vfs.h"
#include "interrupts/port_io.h"
#include "net/net.h"
#include "net/socket.h"
#include "drivers/e1000.h"

// Colors
#define COLOR_BG        0x1E1E1E
#define COLOR_TEXT      0x00FF00
#define COLOR_CURSOR    0x00FF00
#define COLOR_PROMPT    0x00AAFF

// Layout
#define CHAR_WIDTH      8
#define CHAR_HEIGHT     8
#define LINE_HEIGHT     10
#define PROMPT_STR      "> "

// Forward declarations for internal command handlers
static void guiterm_cmd_help(gui_terminal_t *term);
static void guiterm_cmd_clear(gui_terminal_t *term);
static void guiterm_cmd_ls(gui_terminal_t *term, const char *path);
static void guiterm_cmd_cat(gui_terminal_t *term, const char *file);
static void guiterm_cmd_echo(gui_terminal_t *term, const char *text);

// Helper to draw a character to the window
static void draw_term_char(window_t *win, char ch, int x, int y, uint32_t color) {
    if (ch < 32 || ch >= 127) return;
    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if (font_8x8[(int)ch][cy] & (1 << (7 - cx))) {
                window_put_pixel(win, x + cx, y + cy, color);
            }
        }
    }
}

// Helper to draw a string to the window
static void draw_term_string(window_t *win, const char *str, int x, int y, uint32_t color) {
    while (*str) {
        draw_term_char(win, *str, x, y, color);
        x += CHAR_WIDTH;
        str++;
    }
}

// Paint callback
static void guiterminal_paint(window_t *win) {
    gui_terminal_t *term = (gui_terminal_t *)win->user_data;
    if (!term) return;

    // Clear background
    window_clear(win, COLOR_BG);

    // Calculate visible lines
    int visible_lines = (win->content_height - LINE_HEIGHT) / LINE_HEIGHT;
    int start_line = term->scroll_offset;
    int end_line = start_line + visible_lines;
    if (end_line > term->line_count) end_line = term->line_count;

    // Draw output lines
    int y = 2;
    for (int i = start_line; i < end_line; i++) {
        draw_term_string(win, term->lines[i], 4, y, COLOR_TEXT);
        y += LINE_HEIGHT;
    }

    // Draw current input line
    int input_y = win->content_height - LINE_HEIGHT - 2;

    // Draw prompt
    draw_term_string(win, PROMPT_STR, 4, input_y, COLOR_PROMPT);

    // Draw command buffer
    int cmd_x = 4 + strlen(PROMPT_STR) * CHAR_WIDTH;
    draw_term_string(win, term->command_buffer, cmd_x, input_y, COLOR_TEXT);

    // Draw cursor
    int cursor_x = cmd_x + term->cmd_pos * CHAR_WIDTH;
    if (term->cursor_visible) {
        window_fill_rect(win, cursor_x, input_y, 2, CHAR_HEIGHT, COLOR_CURSOR);
    }

    // Draw separator line above input
    window_fill_rect(win, 0, input_y - 4, win->content_width, 1, 0x404040);
}

// Close callback
static void guiterminal_on_close(window_t *win) {
    gui_terminal_t *term = (gui_terminal_t *)win->user_data;
    if (term) {
        kfree(term);
    }
}

// Add a line to the terminal output
static void add_output_line(gui_terminal_t *term, const char *text) {
    // Shift lines up if we're at max
    if (term->line_count >= GUITERM_MAX_LINES) {
        for (int i = 0; i < GUITERM_MAX_LINES - 1; i++) {
            memcpy(term->lines[i], term->lines[i + 1], GUITERM_MAX_LINE_LEN);
        }
        term->line_count = GUITERM_MAX_LINES - 1;
    }

    // Copy text to new line
    int len = strlen(text);
    if (len > GUITERM_MAX_LINE_LEN - 1) len = GUITERM_MAX_LINE_LEN - 1;
    memcpy(term->lines[term->line_count], text, len);
    term->lines[term->line_count][len] = '\0';
    term->line_count++;

    // Auto-scroll to show new line
    int visible_lines = (term->window->content_height - LINE_HEIGHT) / LINE_HEIGHT;
    if (term->line_count > visible_lines) {
        term->scroll_offset = term->line_count - visible_lines;
    }
}

// Forward declarations for commands
static void guiterm_cmd_help(gui_terminal_t *term);
static void guiterm_cmd_clear(gui_terminal_t *term);
static void guiterm_cmd_ls(gui_terminal_t *term, const char *path);
static void guiterm_cmd_cat(gui_terminal_t *term, const char *file);
static void guiterm_cmd_echo(gui_terminal_t *term, const char *text);
static void guiterm_cmd_heap(gui_terminal_t *term);
static void guiterm_cmd_touch(gui_terminal_t *term, const char *name);
static void guiterm_cmd_write(gui_terminal_t *term, const char *args);
static void guiterm_cmd_rm(gui_terminal_t *term, const char *name);
static void guiterm_cmd_reboot(gui_terminal_t *term);
static void guiterm_cmd_wget(gui_terminal_t *term, const char *args);
static void guiterm_cmd_view(gui_terminal_t *term, const char *file);
static void guiterm_cmd_bgimg(gui_terminal_t *term, const char *file);

// Execute a command
static void execute_command(gui_terminal_t *term) {
    // Echo command to output
    char cmd_line[GUITERM_MAX_CMD_LEN + 4];
    cmd_line[0] = '>';
    cmd_line[1] = ' ';
    int len = strlen(term->command_buffer);
    memcpy(cmd_line + 2, term->command_buffer, len);
    cmd_line[len + 2] = '\0';
    add_output_line(term, cmd_line);

    // Parse command
    char cmd[32] = {0};
    char arg[GUITERM_MAX_CMD_LEN] = {0};
    int cmd_pos = 0;
    int arg_pos = 0;
    int in_arg = 0;

    for (int i = 0; term->command_buffer[i]; i++) {
        char c = term->command_buffer[i];
        if (c == ' ' && !in_arg) {
            in_arg = 1;
            continue;
        }
        if (!in_arg && cmd_pos < 31) {
            cmd[cmd_pos++] = c;
        } else if (in_arg && arg_pos < GUITERM_MAX_CMD_LEN - 1) {
            arg[arg_pos++] = c;
        }
    }

    // Execute command
    if (cmd[0] == '\0') {
        // Empty command
    } else if (strcmp(cmd, "help") == 0) {
        guiterm_cmd_help(term);
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        guiterm_cmd_clear(term);
    } else if (strcmp(cmd, "ls") == 0) {
        guiterm_cmd_ls(term, arg[0] ? arg : "/");
    } else if (strcmp(cmd, "cat") == 0) {
        if (arg[0]) {
            guiterm_cmd_cat(term, arg);
        } else {
            add_output_line(term, "Usage: cat <filename>");
        }
    } else if (strcmp(cmd, "echo") == 0) {
        guiterm_cmd_echo(term, arg);
    } else if (strcmp(cmd, "heap") == 0) {
        guiterm_cmd_heap(term);
    } else if (strcmp(cmd, "touch") == 0) {
        guiterm_cmd_touch(term, arg);
    } else if (strcmp(cmd, "write") == 0) {
        guiterm_cmd_write(term, arg);
    } else if (strcmp(cmd, "rm") == 0) {
        guiterm_cmd_rm(term, arg);
    } else if (strcmp(cmd, "reboot") == 0) {
        guiterm_cmd_reboot(term);
    } else if (strcmp(cmd, "wget") == 0) {
        guiterm_cmd_wget(term, arg);
    } else if (strcmp(cmd, "view") == 0) {
        if (arg[0]) {
            guiterm_cmd_view(term, arg);
        } else {
            add_output_line(term, "Usage: view <image_file>");
        }
    } else if (strcmp(cmd, "bgimg") == 0) {
        if (arg[0]) {
            guiterm_cmd_bgimg(term, arg);
        } else {
            add_output_line(term, "Usage: bgimg <image_file>");
        }
    } else if (strcmp(cmd, "exit") == 0) {
        window_destroy(term->window);
        return;
    } else {
        char err[80] = "Unknown command: ";
        int err_len = strlen(err);
        int cmd_len = strlen(cmd);
        if (cmd_len > 60) cmd_len = 60;
        memcpy(err + err_len, cmd, cmd_len);
        err[err_len + cmd_len] = '\0';
        add_output_line(term, err);
    }

    // Clear command buffer
    memset(term->command_buffer, 0, GUITERM_MAX_CMD_LEN);
    term->cmd_pos = 0;

    window_invalidate(term->window);
}

// Command handlers
static void guiterm_cmd_help(gui_terminal_t *term) {
    add_output_line(term, "Available commands:");
    add_output_line(term, "  help   - Show this help");
    add_output_line(term, "  clear  - Clear screen");
    add_output_line(term, "  ls     - List directory");
    add_output_line(term, "  cat    - Show file contents");
    add_output_line(term, "  touch  - Create file");
    add_output_line(term, "  write  - Write to file");
    add_output_line(term, "  rm     - Remove file");
    add_output_line(term, "  echo   - Print text");
    add_output_line(term, "  heap   - Show memory stats");
    add_output_line(term, "  wget   - Download file from web");
    add_output_line(term, "  view   - View image file");
    add_output_line(term, "  bgimg  - Set desktop background");
    add_output_line(term, "  reboot - Reboot system");
    add_output_line(term, "  exit   - Close terminal");
}

static void guiterm_cmd_clear(gui_terminal_t *term) {
    term->line_count = 0;
    term->scroll_offset = 0;
    memset(term->lines, 0, sizeof(term->lines));
}

static void guiterm_cmd_ls(gui_terminal_t *term, const char *path) {
    vfs_node_t *dir = vfs_resolve_path(path);
    if (!dir) {
        add_output_line(term, "Directory not found");
        return;
    }

    if (!(dir->flags & VFS_DIRECTORY)) {
        add_output_line(term, "Not a directory");
        return;
    }

    char header[80] = "Contents of ";
    int hlen = strlen(header);
    int plen = strlen(path);
    if (plen > 60) plen = 60;
    memcpy(header + hlen, path, plen);
    header[hlen + plen] = ':';
    header[hlen + plen + 1] = '\0';
    add_output_line(term, header);

    uint32_t i = 0;
    vfs_node_t *child;
    int count = 0;

    while (i < 64 && (child = vfs_readdir(dir, i)) != 0) {
        char line[80] = "  ";
        if (child->flags & VFS_DIRECTORY) {
            memcpy(line + 2, "[DIR]  ", 7);
            memcpy(line + 9, child->name, strlen(child->name));
        } else {
            memcpy(line + 2, "[FILE] ", 7);
            memcpy(line + 9, child->name, strlen(child->name));
        }
        add_output_line(term, line);
        kfree(child);
        count++;
        i++;
    }

    if (count == 0) {
        add_output_line(term, "  (empty)");
    }
}

static void guiterm_cmd_cat(gui_terminal_t *term, const char *file) {
    vfs_node_t *f = vfs_open(file, VFS_READ);
    if (!f) {
        add_output_line(term, "File not found");
        return;
    }

    uint8_t buffer[512];
    int bytes = vfs_read(f, 0, 512, buffer);
    vfs_close(f);

    if (bytes <= 0) {
        add_output_line(term, "(empty file)");
        return;
    }

    // Split into lines
    char line[GUITERM_MAX_LINE_LEN];
    int line_pos = 0;

    for (int i = 0; i < bytes; i++) {
        if (buffer[i] == '\n' || line_pos >= GUITERM_MAX_LINE_LEN - 1) {
            line[line_pos] = '\0';
            add_output_line(term, line);
            line_pos = 0;
        } else if (buffer[i] >= 32 && buffer[i] < 127) {
            line[line_pos++] = buffer[i];
        }
    }

    // Add remaining text
    if (line_pos > 0) {
        line[line_pos] = '\0';
        add_output_line(term, line);
    }
}

static void guiterm_cmd_echo(gui_terminal_t *term, const char *text) {
    if (text && text[0]) {
        add_output_line(term, text);
    }
}

static void guiterm_cmd_heap(gui_terminal_t *term) {
    uint32_t total, used, free_mem;
    heap_stats(&total, &used, &free_mem);

    char line[60];

    add_output_line(term, "Heap Statistics:");

    // Format: "  Total: XXXXX KB"
    int pos = 0;
    line[pos++] = ' '; line[pos++] = ' ';
    line[pos++] = 'T'; line[pos++] = 'o'; line[pos++] = 't';
    line[pos++] = 'a'; line[pos++] = 'l'; line[pos++] = ':';
    line[pos++] = ' ';
    uint32_t kb = total / 1024;
    if (kb >= 10000) line[pos++] = '0' + (kb / 10000) % 10;
    if (kb >= 1000) line[pos++] = '0' + (kb / 1000) % 10;
    if (kb >= 100) line[pos++] = '0' + (kb / 100) % 10;
    if (kb >= 10) line[pos++] = '0' + (kb / 10) % 10;
    line[pos++] = '0' + kb % 10;
    line[pos++] = ' '; line[pos++] = 'K'; line[pos++] = 'B';
    line[pos] = '\0';
    add_output_line(term, line);

    pos = 0;
    line[pos++] = ' '; line[pos++] = ' ';
    line[pos++] = 'U'; line[pos++] = 's'; line[pos++] = 'e';
    line[pos++] = 'd'; line[pos++] = ':'; line[pos++] = ' ';
    line[pos++] = ' ';
    kb = used / 1024;
    if (kb >= 10000) line[pos++] = '0' + (kb / 10000) % 10;
    if (kb >= 1000) line[pos++] = '0' + (kb / 1000) % 10;
    if (kb >= 100) line[pos++] = '0' + (kb / 100) % 10;
    if (kb >= 10) line[pos++] = '0' + (kb / 10) % 10;
    line[pos++] = '0' + kb % 10;
    line[pos++] = ' '; line[pos++] = 'K'; line[pos++] = 'B';
    line[pos] = '\0';
    add_output_line(term, line);

    pos = 0;
    line[pos++] = ' '; line[pos++] = ' ';
    line[pos++] = 'F'; line[pos++] = 'r'; line[pos++] = 'e';
    line[pos++] = 'e'; line[pos++] = ':'; line[pos++] = ' ';
    line[pos++] = ' ';
    kb = free_mem / 1024;
    if (kb >= 10000) line[pos++] = '0' + (kb / 10000) % 10;
    if (kb >= 1000) line[pos++] = '0' + (kb / 1000) % 10;
    if (kb >= 100) line[pos++] = '0' + (kb / 100) % 10;
    if (kb >= 10) line[pos++] = '0' + (kb / 10) % 10;
    line[pos++] = '0' + kb % 10;
    line[pos++] = ' '; line[pos++] = 'K'; line[pos++] = 'B';
    line[pos] = '\0';
    add_output_line(term, line);
}

static void guiterm_cmd_touch(gui_terminal_t *term, const char *name) {
    if (!name || !name[0]) {
        add_output_line(term, "Usage: touch <filename>");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root) {
        add_output_line(term, "Error: No filesystem");
        return;
    }

    // Check if exists
    vfs_node_t *existing = vfs_finddir(root, name);
    if (existing) {
        add_output_line(term, "File already exists");
        kfree(existing);
        return;
    }

    if (root->create) {
        if (root->create(root, name, VFS_FILE) == 0) {
            add_output_line(term, "File created");
        } else {
            add_output_line(term, "Error creating file");
        }
    } else {
        add_output_line(term, "Create not supported");
    }
}

static void guiterm_cmd_write(gui_terminal_t *term, const char *args) {
    if (!args || !args[0]) {
        add_output_line(term, "Usage: write <file> <text>");
        return;
    }

    // Parse filename and content
    char filename[32] = {0};
    char content[128] = {0};
    int i = 0, j = 0;

    // Get filename
    while (args[i] && args[i] != ' ' && j < 31) {
        filename[j++] = args[i++];
    }
    filename[j] = '\0';

    // Skip space
    while (args[i] == ' ') i++;

    // Get content
    j = 0;
    while (args[i] && j < 127) {
        content[j++] = args[i++];
    }
    content[j] = '\0';

    if (!filename[0] || !content[0]) {
        add_output_line(term, "Usage: write <file> <text>");
        return;
    }

    vfs_node_t *file = vfs_open(filename, VFS_WRITE);
    if (!file) {
        add_output_line(term, "File not found");
        return;
    }

    int written = vfs_write(file, 0, strlen(content), (uint8_t *)content);
    vfs_close(file);

    if (written > 0) {
        add_output_line(term, "Written successfully");
    } else {
        add_output_line(term, "Write failed");
    }
}

static void guiterm_cmd_rm(gui_terminal_t *term, const char *name) {
    if (!name || !name[0]) {
        add_output_line(term, "Usage: rm <filename>");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root) {
        add_output_line(term, "Error: No filesystem");
        return;
    }

    if (root->delete) {
        if (root->delete(root, name) == 0) {
            add_output_line(term, "File removed");
        } else {
            add_output_line(term, "Error removing file");
        }
    } else {
        add_output_line(term, "Delete not supported");
    }
}

static void guiterm_cmd_reboot(gui_terminal_t *term) {
    add_output_line(term, "Rebooting...");
    window_invalidate(term->window);

    // Wait a bit then reboot
    for (volatile int i = 0; i < 1000000; i++);

    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

// Helper to parse IP address
static uint32_t guiterm_parse_ip(const char *str) {
    if (!str || !*str) return 0;

    uint8_t octets[4] = {0};
    int octet_idx = 0;
    int value = 0;
    int has_digit = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            value = value * 10 + (*str - '0');
            has_digit = 1;
            if (value > 255) return 0;
        } else if (*str == '.') {
            if (!has_digit || octet_idx >= 3) return 0;
            octets[octet_idx++] = value;
            value = 0;
            has_digit = 0;
        } else {
            return 0;
        }
        str++;
    }

    if (!has_digit || octet_idx != 3) return 0;
    octets[octet_idx] = value;

    return ip_to_uint32(octets[0], octets[1], octets[2], octets[3]);
}

static void guiterm_cmd_wget(gui_terminal_t *term, const char *args) {
    if (!args || !args[0]) {
        add_output_line(term, "Usage: wget <ip> <port> <path> [outfile]");
        add_output_line(term, "Example: wget 10.0.2.2 80 /file.txt out.txt");
        return;
    }

    // Parse arguments: ip port path [outfile]
    char ip_str[20] = {0};
    char port_str[10] = {0};
    char path[64] = {0};
    char outfile[32] = {0};

    int i = 0, j = 0;

    // Get IP
    while (args[i] && args[i] != ' ' && j < 19) {
        ip_str[j++] = args[i++];
    }
    while (args[i] == ' ') i++;

    // Get port
    j = 0;
    while (args[i] && args[i] != ' ' && j < 9) {
        port_str[j++] = args[i++];
    }
    while (args[i] == ' ') i++;

    // Get path
    j = 0;
    while (args[i] && args[i] != ' ' && j < 63) {
        path[j++] = args[i++];
    }
    while (args[i] == ' ') i++;

    // Get optional outfile
    j = 0;
    while (args[i] && j < 31) {
        outfile[j++] = args[i++];
    }

    if (!ip_str[0] || !port_str[0] || !path[0]) {
        add_output_line(term, "Invalid arguments");
        return;
    }

    uint32_t ip = guiterm_parse_ip(ip_str);
    if (ip == 0) {
        add_output_line(term, "Invalid IP address");
        return;
    }

    int port = 0;
    for (int k = 0; port_str[k]; k++) {
        port = port * 10 + (port_str[k] - '0');
    }

    add_output_line(term, "Connecting...");
    window_invalidate(term->window);

    // Allocate buffer
    #define WGET_BUF_SIZE (256 * 1024)
    uint8_t *response = (uint8_t *)kmalloc(WGET_BUF_SIZE);
    if (!response) {
        add_output_line(term, "Out of memory");
        return;
    }

    int len = http_get(ip_str, (uint16_t)port, path, (char *)response, WGET_BUF_SIZE);

    if (len > 0) {
        // Find end of HTTP headers
        uint8_t *body = response;
        int body_len = len;
        for (int k = 0; k < len - 3; k++) {
            if (response[k] == '\r' && response[k+1] == '\n' &&
                response[k+2] == '\r' && response[k+3] == '\n') {
                body = response + k + 4;
                body_len = len - (k + 4);
                break;
            }
        }

        char msg[60];
        int pos = 0;
        const char *recv = "Received ";
        while (*recv) msg[pos++] = *recv++;
        int bl = body_len;
        if (bl >= 10000) msg[pos++] = '0' + (bl / 10000) % 10;
        if (bl >= 1000) msg[pos++] = '0' + (bl / 1000) % 10;
        if (bl >= 100) msg[pos++] = '0' + (bl / 100) % 10;
        if (bl >= 10) msg[pos++] = '0' + (bl / 10) % 10;
        msg[pos++] = '0' + bl % 10;
        const char *bytes = " bytes";
        while (*bytes) msg[pos++] = *bytes++;
        msg[pos] = '\0';
        add_output_line(term, msg);

        // Save to file if outfile specified
        if (outfile[0]) {
            vfs_node_t *root = vfs_get_root();
            if (root) {
                // Create file if needed
                const char *fname = outfile[0] == '/' ? outfile + 1 : outfile;
                vfs_node_t *file = vfs_finddir(root, fname);
                if (!file && root->create) {
                    root->create(root, fname, VFS_FILE);
                    file = vfs_finddir(root, fname);
                }
                if (file) {
                    vfs_write(file, 0, body_len, body);
                    vfs_close(file);
                    add_output_line(term, "Saved to file");
                    kfree(file);
                }
            }
        }
    } else {
        add_output_line(term, "Download failed");
    }

    kfree(response);
}

static void guiterm_cmd_view(gui_terminal_t *term, const char *file) {
    if (!file || !file[0]) {
        add_output_line(term, "Usage: view <image_file>");
        return;
    }

    add_output_line(term, "Opening image viewer...");
    window_invalidate(term->window);

    image_viewer_t *viewer = imageviewer_create("Image Viewer", 50, 50);
    if (!viewer) {
        add_output_line(term, "Failed to create viewer");
        return;
    }

    if (imageviewer_load_file(viewer, file) != 0) {
        add_output_line(term, "Failed to load image");
        imageviewer_close(viewer);
        return;
    }

    add_output_line(term, "Image loaded");
}

static void guiterm_cmd_bgimg(gui_terminal_t *term, const char *file) {
    if (!file || !file[0]) {
        add_output_line(term, "Usage: bgimg <image_file>");
        return;
    }

    add_output_line(term, "Setting background...");
    window_invalidate(term->window);

    if (desktop_set_background(file) == 0) {
        add_output_line(term, "Background set");
    } else {
        add_output_line(term, "Failed to set background");
    }
}

// Create GUI terminal
gui_terminal_t *guiterminal_create(const char *title, int32_t x, int32_t y) {
    gui_terminal_t *term = (gui_terminal_t *)kcalloc(1, sizeof(gui_terminal_t));
    if (!term) return 0;

    // Create window
    term->window = window_create(title, x, y,
                                 GUITERM_WIDTH, GUITERM_HEIGHT,
                                 WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE |
                                 WINDOW_FLAG_CLOSABLE);

    if (!term->window) {
        kfree(term);
        return 0;
    }

    // Set callbacks and user data
    term->window->user_data = term;
    term->window->on_paint = guiterminal_paint;
    term->window->on_close = guiterminal_on_close;

    // Initialize state
    term->line_count = 0;
    term->scroll_offset = 0;
    term->cmd_pos = 0;
    term->cursor_visible = 1;

    // Welcome message
    add_output_line(term, "GUI Terminal v1.0");
    add_output_line(term, "Type 'help' for commands");
    add_output_line(term, "");

    return term;
}

// Handle keyboard input
void guiterminal_handle_key(gui_terminal_t *term, char c) {
    if (!term) return;

    if (c == '\n') {
        // Execute command
        execute_command(term);
    } else if (c == '\b') {
        // Backspace
        if (term->cmd_pos > 0) {
            term->cmd_pos--;
            // Shift remaining characters left
            for (int i = term->cmd_pos; term->command_buffer[i]; i++) {
                term->command_buffer[i] = term->command_buffer[i + 1];
            }
        }
    } else if (c >= 32 && c < 127) {
        // Printable character
        if (term->cmd_pos < GUITERM_MAX_CMD_LEN - 1) {
            term->command_buffer[term->cmd_pos++] = c;
            term->command_buffer[term->cmd_pos] = '\0';
        }
    }

    window_invalidate(term->window);
}

// Print text to terminal
void guiterminal_print(gui_terminal_t *term, const char *text) {
    if (!term || !text) return;

    // Split text into lines
    char line[GUITERM_MAX_LINE_LEN];
    int line_pos = 0;

    while (*text) {
        if (*text == '\n' || line_pos >= GUITERM_MAX_LINE_LEN - 1) {
            line[line_pos] = '\0';
            add_output_line(term, line);
            line_pos = 0;
        } else {
            line[line_pos++] = *text;
        }
        text++;
    }

    // Add remaining text
    if (line_pos > 0) {
        line[line_pos] = '\0';
        add_output_line(term, line);
    }

    window_invalidate(term->window);
}

// Print line to terminal
void guiterminal_println(gui_terminal_t *term, const char *text) {
    if (!term) return;
    if (text) {
        add_output_line(term, text);
    } else {
        add_output_line(term, "");
    }
    window_invalidate(term->window);
}

// Clear terminal
void guiterminal_clear(gui_terminal_t *term) {
    if (term) {
        guiterm_cmd_clear(term);
        window_invalidate(term->window);
    }
}

// Close terminal
void guiterminal_close(gui_terminal_t *term) {
    if (term && term->window) {
        window_destroy(term->window);
    }
}

// Refresh display
void guiterminal_refresh(gui_terminal_t *term) {
    if (term && term->window) {
        window_invalidate(term->window);
    }
}
