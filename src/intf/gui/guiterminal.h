#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gui/window.h"

// GUI Terminal constants
#define GUITERM_WIDTH 400
#define GUITERM_HEIGHT 220
#define GUITERM_MAX_LINES 50
#define GUITERM_MAX_LINE_LEN 60
#define GUITERM_MAX_CMD_LEN 128

// GUI Terminal structure
typedef struct gui_terminal {
    window_t *window;
    char lines[GUITERM_MAX_LINES][GUITERM_MAX_LINE_LEN];
    int line_count;
    int scroll_offset;
    char command_buffer[GUITERM_MAX_CMD_LEN];
    int cmd_pos;
    int cursor_visible;
} gui_terminal_t;

// Create a new GUI terminal window
gui_terminal_t *guiterminal_create(const char *title, int32_t x, int32_t y);

// Close the terminal
void guiterminal_close(gui_terminal_t *term);

// Handle keyboard input
void guiterminal_handle_key(gui_terminal_t *term, char c);

// Print text to terminal
void guiterminal_print(gui_terminal_t *term, const char *text);

// Print line to terminal (with newline)
void guiterminal_println(gui_terminal_t *term, const char *text);

// Clear terminal
void guiterminal_clear(gui_terminal_t *term);

// Refresh the display
void guiterminal_refresh(gui_terminal_t *term);
