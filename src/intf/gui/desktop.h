#pragma once
#include <stdint.h>
#include <stddef.h>

// Forward declarations
struct window;
typedef struct window window_t;
struct text_editor;
typedef struct text_editor text_editor_t;
struct draw_app;
typedef struct draw_app draw_app_t;
struct gui_terminal;
typedef struct gui_terminal gui_terminal_t;

// Desktop constants
#define TASKBAR_HEIGHT 32
#define TASKBAR_BUTTON_WIDTH 80
#define TASKBAR_BUTTON_HEIGHT 24
#define TASKBAR_PADDING 4

// Desktop colors
#define DESKTOP_BG_COLOR    0x008080
#define TASKBAR_BG_COLOR    0x2D2D2D
#define TASKBAR_BTN_COLOR   0x404040
#define TASKBAR_BTN_HOVER   0x505050
#define TASKBAR_BTN_TEXT    0xFFFFFF

// App types
typedef enum {
    APP_NONE,
    APP_EDITOR,
    APP_DRAW,
    APP_TERMINAL
} app_type_t;

// Desktop state
typedef struct desktop {
    int active;

    // App instances (one of each max for simplicity)
    text_editor_t *editor;
    draw_app_t *drawapp;
    gui_terminal_t *terminal;

    // Taskbar button hover state
    int hover_button;  // -1 = none, 0 = editor, 1 = draw, 2 = terminal

    // Background image (optional)
    uint32_t *bg_pixels;
    uint32_t bg_width;
    uint32_t bg_height;
} desktop_t;

// Initialize the desktop environment
void desktop_init(void);

// Shutdown desktop
void desktop_shutdown(void);

// Check if desktop is active
int desktop_is_active(void);

// Handle keyboard input (returns 1 if handled)
int desktop_handle_key(char c);

// Handle mouse input (returns 1 if handled)
int desktop_handle_mouse(int32_t x, int32_t y, uint8_t buttons);

// Render the desktop (taskbar, background)
void desktop_render(void);

// Open an application
void desktop_open_app(app_type_t app);

// Get the currently focused app type
app_type_t desktop_get_focused_app(void);

// Notify desktop that a window was closed (for cleanup)
void desktop_notify_window_closed(window_t *win);

// Set desktop background image from file
int desktop_set_background(const char *path);
