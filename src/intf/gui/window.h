#pragma once
#include <stdint.h>
#include <stddef.h>

// Window flags
#define WINDOW_FLAG_VISIBLE    0x01
#define WINDOW_FLAG_MOVABLE    0x02
#define WINDOW_FLAG_CLOSABLE   0x04
#define WINDOW_FLAG_FOCUSED    0x08
#define WINDOW_FLAG_DIRTY      0x10

// Window title bar height
#define WINDOW_TITLE_HEIGHT 20

// Window border size
#define WINDOW_BORDER_SIZE 2

// Maximum windows
#define MAX_WINDOWS 16

// Maximum title length
#define MAX_TITLE_LENGTH 64

// Window structure
typedef struct window {
    int32_t x, y;               // Position
    uint32_t width, height;     // Size (including decorations)
    uint32_t content_width;     // Content area size
    uint32_t content_height;
    char title[MAX_TITLE_LENGTH];
    uint32_t flags;
    uint32_t *framebuffer;      // Window's framebuffer (content only)
    int id;
    int z_order;                // Higher = on top

    // Callback for rendering content
    void (*on_paint)(struct window *win);
    void (*on_close)(struct window *win);

    // User data
    void *user_data;
} window_t;

// Window manager initialization
void wm_init(void);

// Window creation/destruction
window_t *window_create(const char *title, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, uint32_t flags);
void window_destroy(window_t *win);

// Window operations
void window_show(window_t *win);
void window_hide(window_t *win);
void window_move(window_t *win, int32_t x, int32_t y);
void window_resize(window_t *win, uint32_t width, uint32_t height);
void window_focus(window_t *win);
void window_set_title(window_t *win, const char *title);
void window_invalidate(window_t *win);

// Drawing to window content
void window_clear(window_t *win, uint32_t color);
void window_put_pixel(window_t *win, uint32_t x, uint32_t y, uint32_t color);
void window_fill_rect(window_t *win, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t color);
void window_draw_text(window_t *win, const char *text,
                      uint32_t x, uint32_t y, uint32_t color);
void window_draw_image(window_t *win, const uint32_t *pixels,
                       uint32_t x, uint32_t y, uint32_t w, uint32_t h);

// Window manager event handling
void wm_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void wm_render(void);
int wm_needs_render(void);  // Returns 1 if any window needs redrawing
void wm_force_render(void); // Force full redraw
window_t *wm_get_window_at(int32_t x, int32_t y);
window_t *wm_get_focused_window(void);
int wm_is_dragging(void);   // Returns 1 if currently dragging a window

// Color constants for window decorations
#define WM_COLOR_TITLE_ACTIVE   0x4488CC
#define WM_COLOR_TITLE_INACTIVE 0x666666
#define WM_COLOR_TITLE_TEXT     0xFFFFFF
#define WM_COLOR_BORDER         0x333333
#define WM_COLOR_CLOSE_BTN      0xFF4444
#define WM_COLOR_BACKGROUND     0xEEEEEE
