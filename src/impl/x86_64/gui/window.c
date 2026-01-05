#include "gui/window.h"
#include "graphics/graphics.h"
#include "graphics/font.h"
#include "graphics/cursor.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "utils/string.h"

// Desktop background color
#define DESKTOP_COLOR 0x008080

// Window list
static window_t *windows[MAX_WINDOWS];
static int window_count = 0;
static int next_window_id = 1;

// Drag state
static window_t *drag_window = 0;
static int32_t drag_offset_x = 0;
static int32_t drag_offset_y = 0;
static int32_t drag_outline_x = 0;  // Current outline position
static int32_t drag_outline_y = 0;
static int32_t drag_start_x = 0;    // Original window position
static int32_t drag_start_y = 0;
static uint8_t last_buttons = 0;

// Focused window
static window_t *focused_window = 0;

// Global dirty flag for full redraw
static int wm_global_dirty = 1;

void wm_init(void) {
    memset(windows, 0, sizeof(windows));
    window_count = 0;
    next_window_id = 1;
    drag_window = 0;
    focused_window = 0;
    wm_global_dirty = 1;
}

// Find free window slot
static int find_free_slot(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i] == 0) {
            return i;
        }
    }
    return -1;
}

// Get highest z-order
static int get_highest_z(void) {
    int highest = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i] && windows[i]->z_order > highest) {
            highest = windows[i]->z_order;
        }
    }
    return highest;
}

// Create window
window_t *window_create(const char *title, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, uint32_t flags) {
    int slot = find_free_slot();
    if (slot < 0) return 0;

    window_t *win = (window_t *)kcalloc(1, sizeof(window_t));
    if (!win) return 0;

    win->x = x;
    win->y = y;
    win->content_width = width;
    win->content_height = height;
    win->width = width + WINDOW_BORDER_SIZE * 2;
    win->height = height + WINDOW_TITLE_HEIGHT + WINDOW_BORDER_SIZE;
    win->flags = flags | WINDOW_FLAG_DIRTY;
    win->id = next_window_id++;
    win->z_order = get_highest_z() + 1;

    // Copy title
    size_t title_len = strlen(title);
    if (title_len > MAX_TITLE_LENGTH - 1) title_len = MAX_TITLE_LENGTH - 1;
    memcpy(win->title, title, title_len);
    win->title[title_len] = 0;

    // Allocate framebuffer for content
    win->framebuffer = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    if (!win->framebuffer) {
        kfree(win);
        return 0;
    }

    // Clear to default background
    for (uint32_t i = 0; i < width * height; i++) {
        win->framebuffer[i] = WM_COLOR_BACKGROUND;
    }

    windows[slot] = win;
    window_count++;

    // Focus new window (this also marks it dirty so it will be rendered)
    window_focus(win);

    return win;
}

// Destroy window
void window_destroy(window_t *win) {
    if (!win) return;

    // Call close callback
    if (win->on_close) {
        win->on_close(win);
    }

    // Remove from window list
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i] == win) {
            windows[i] = 0;
            window_count--;
            break;
        }
    }

    // Clear focus/drag if needed
    if (focused_window == win) focused_window = 0;
    if (drag_window == win) drag_window = 0;

    // Free resources
    if (win->framebuffer) {
        kfree(win->framebuffer);
    }
    kfree(win);

    // Need full redraw to clear where window was
    wm_global_dirty = 1;
}

void window_show(window_t *win) {
    if (win) {
        win->flags |= WINDOW_FLAG_VISIBLE | WINDOW_FLAG_DIRTY;
    }
}

void window_hide(window_t *win) {
    if (win) {
        win->flags &= ~WINDOW_FLAG_VISIBLE;
    }
}

void window_move(window_t *win, int32_t x, int32_t y) {
    if (win) {
        win->x = x;
        win->y = y;
        win->flags |= WINDOW_FLAG_DIRTY;
    }
}

void window_resize(window_t *win, uint32_t width, uint32_t height) {
    if (!win) return;

    // Reallocate framebuffer
    uint32_t *new_fb = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    if (!new_fb) return;

    // Clear new framebuffer
    for (uint32_t i = 0; i < width * height; i++) {
        new_fb[i] = WM_COLOR_BACKGROUND;
    }

    kfree(win->framebuffer);
    win->framebuffer = new_fb;

    win->content_width = width;
    win->content_height = height;
    win->width = width + WINDOW_BORDER_SIZE * 2;
    win->height = height + WINDOW_TITLE_HEIGHT + WINDOW_BORDER_SIZE;
    win->flags |= WINDOW_FLAG_DIRTY;
}

void window_focus(window_t *win) {
    if (!win) return;

    // Unfocus previous
    if (focused_window) {
        focused_window->flags &= ~WINDOW_FLAG_FOCUSED;
        focused_window->flags |= WINDOW_FLAG_DIRTY;
    }

    // Focus new window
    win->flags |= WINDOW_FLAG_FOCUSED | WINDOW_FLAG_DIRTY;
    focused_window = win;

    // Bring to top
    win->z_order = get_highest_z() + 1;
}

void window_set_title(window_t *win, const char *title) {
    if (!win) return;

    size_t title_len = strlen(title);
    if (title_len > MAX_TITLE_LENGTH - 1) title_len = MAX_TITLE_LENGTH - 1;
    memcpy(win->title, title, title_len);
    win->title[title_len] = 0;
    win->flags |= WINDOW_FLAG_DIRTY;
}

void window_invalidate(window_t *win) {
    if (win) {
        win->flags |= WINDOW_FLAG_DIRTY;
    }
}

// Drawing functions
void window_clear(window_t *win, uint32_t color) {
    if (!win || !win->framebuffer) return;

    for (uint32_t i = 0; i < win->content_width * win->content_height; i++) {
        win->framebuffer[i] = color;
    }
    win->flags |= WINDOW_FLAG_DIRTY;
}

void window_put_pixel(window_t *win, uint32_t x, uint32_t y, uint32_t color) {
    if (!win || !win->framebuffer) return;
    if (x >= win->content_width || y >= win->content_height) return;

    win->framebuffer[y * win->content_width + x] = color;
}

void window_fill_rect(window_t *win, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t color) {
    if (!win || !win->framebuffer) return;

    for (uint32_t py = y; py < y + h && py < win->content_height; py++) {
        for (uint32_t px = x; px < x + w && px < win->content_width; px++) {
            win->framebuffer[py * win->content_width + px] = color;
        }
    }
    win->flags |= WINDOW_FLAG_DIRTY;
}

void window_draw_text(window_t *win, const char *text,
                      uint32_t x, uint32_t y, uint32_t color) {
    if (!win || !win->framebuffer || !text) return;

    uint32_t cur_x = x;
    while (*text) {
        if (*text == '\n') {
            y += 8;
            cur_x = x;
        } else {
            // Draw character to window's framebuffer
            for (int cy = 0; cy < 8 && y + cy < win->content_height; cy++) {
                for (int cx = 0; cx < 8 && cur_x + cx < win->content_width; cx++) {
                    // This would need access to font data
                    // For now, simplified version
                }
            }
            cur_x += 8;
        }
        text++;
    }
    win->flags |= WINDOW_FLAG_DIRTY;
}

void window_draw_image(window_t *win, const uint32_t *pixels,
                       uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!win || !win->framebuffer || !pixels) return;

    for (uint32_t py = 0; py < h && y + py < win->content_height; py++) {
        for (uint32_t px = 0; px < w && x + px < win->content_width; px++) {
            win->framebuffer[(y + py) * win->content_width + (x + px)] = pixels[py * w + px];
        }
    }
    win->flags |= WINDOW_FLAG_DIRTY;
}

// Check if point is in window's title bar
static int point_in_title_bar(window_t *win, int32_t x, int32_t y) {
    return x >= win->x && x < win->x + (int32_t)win->width &&
           y >= win->y && y < win->y + WINDOW_TITLE_HEIGHT;
}

// Check if point is in close button
static int point_in_close_button(window_t *win, int32_t x, int32_t y) {
    int32_t btn_x = win->x + win->width - WINDOW_TITLE_HEIGHT;
    int32_t btn_y = win->y;
    return x >= btn_x && x < btn_x + WINDOW_TITLE_HEIGHT &&
           y >= btn_y && y < btn_y + WINDOW_TITLE_HEIGHT;
}

// Get window at point (top-most)
window_t *wm_get_window_at(int32_t x, int32_t y) {
    window_t *top = 0;
    int top_z = -1;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t *win = windows[i];
        if (!win || !(win->flags & WINDOW_FLAG_VISIBLE)) continue;

        if (x >= win->x && x < win->x + (int32_t)win->width &&
            y >= win->y && y < win->y + (int32_t)win->height) {
            if (win->z_order > top_z) {
                top = win;
                top_z = win->z_order;
            }
        }
    }

    return top;
}

window_t *wm_get_focused_window(void) {
    return focused_window;
}

int wm_is_dragging(void) {
    return drag_window != 0;
}

int wm_needs_render(void) {
    if (wm_global_dirty) return 1;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i] && (windows[i]->flags & WINDOW_FLAG_DIRTY)) {
            return 1;
        }
    }
    return 0;
}

void wm_force_render(void) {
    wm_global_dirty = 1;
}

// Draw XOR outline rectangle (calling twice erases it)
static void draw_xor_outline(int32_t x, int32_t y, uint32_t w, uint32_t h) {
    uint32_t screen_w = get_screen_width();
    uint32_t screen_h = get_screen_height();

    // Draw horizontal lines (top and bottom)
    for (uint32_t px = 0; px < w; px++) {
        int32_t sx = x + px;
        if (sx >= 0 && sx < (int32_t)screen_w) {
            // Top edge
            if (y >= 0 && y < (int32_t)screen_h) {
                uint32_t color = get_pixel_color(sx, y);
                put_pixel(sx, y, color ^ 0xFFFFFF);
            }
            // Bottom edge
            int32_t by = y + h - 1;
            if (by >= 0 && by < (int32_t)screen_h) {
                uint32_t color = get_pixel_color(sx, by);
                put_pixel(sx, by, color ^ 0xFFFFFF);
            }
        }
    }

    // Draw vertical lines (left and right)
    for (uint32_t py = 1; py < h - 1; py++) {
        int32_t sy = y + py;
        if (sy >= 0 && sy < (int32_t)screen_h) {
            // Left edge
            if (x >= 0 && x < (int32_t)screen_w) {
                uint32_t color = get_pixel_color(x, sy);
                put_pixel(x, sy, color ^ 0xFFFFFF);
            }
            // Right edge
            int32_t rx = x + w - 1;
            if (rx >= 0 && rx < (int32_t)screen_w) {
                uint32_t color = get_pixel_color(rx, sy);
                put_pixel(rx, sy, color ^ 0xFFFFFF);
            }
        }
    }
}

// Handle mouse input
void wm_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    uint8_t left_pressed = (buttons & 0x01) && !(last_buttons & 0x01);
    uint8_t left_released = !(buttons & 0x01) && (last_buttons & 0x01);
    uint8_t left_held = buttons & 0x01;

    // Handle drag - only update outline position, don't move window
    if (drag_window && left_held) {
        int32_t new_x = x - drag_offset_x;
        int32_t new_y = y - drag_offset_y;

        // Only update if position changed
        if (new_x != drag_outline_x || new_y != drag_outline_y) {
            // Erase old outline
            draw_xor_outline(drag_outline_x, drag_outline_y,
                            drag_window->width, drag_window->height);
            // Update position
            drag_outline_x = new_x;
            drag_outline_y = new_y;
            // Draw new outline
            draw_xor_outline(drag_outline_x, drag_outline_y,
                            drag_window->width, drag_window->height);
        }
    }

    // Handle release - move window to final position
    if (left_released && drag_window) {
        // Erase final outline
        draw_xor_outline(drag_outline_x, drag_outline_y,
                        drag_window->width, drag_window->height);
        // Move window to final position
        window_move(drag_window, drag_outline_x, drag_outline_y);
        // Force full redraw
        wm_global_dirty = 1;
        drag_window = 0;
    }

    // Handle press
    if (left_pressed) {
        window_t *win = wm_get_window_at(x, y);

        if (win) {
            window_focus(win);

            // Check close button
            if ((win->flags & WINDOW_FLAG_CLOSABLE) && point_in_close_button(win, x, y)) {
                window_destroy(win);
            }
            // Check title bar for drag
            else if ((win->flags & WINDOW_FLAG_MOVABLE) && point_in_title_bar(win, x, y)) {
                drag_window = win;
                drag_offset_x = x - win->x;
                drag_offset_y = y - win->y;
                // Save original position and initialize outline
                drag_start_x = win->x;
                drag_start_y = win->y;
                drag_outline_x = win->x;
                drag_outline_y = win->y;
                // Draw initial outline
                draw_xor_outline(drag_outline_x, drag_outline_y,
                                win->width, win->height);
            }
        }
    }

    last_buttons = buttons;
}

// Render all windows
void wm_render(void) {
    // Skip rendering while dragging (using XOR outline instead)
    if (drag_window) {
        return;
    }

    // Handle case where all windows were destroyed
    if (wm_global_dirty && window_count == 0) {
        cursor_hide();
        clear_screen(COLOR_BLACK);  // Clear to shell background
        wm_global_dirty = 0;
        cursor_show();
        return;
    }

    // Skip rendering if nothing needs to be drawn or no windows
    if (!wm_needs_render() || window_count == 0) {
        return;
    }

    // Hide cursor before rendering (so we don't save window content as background)
    cursor_hide();

    // If global dirty (from drag end), clear desktop to erase old window position
    if (wm_global_dirty) {
        clear_screen(DESKTOP_COLOR);
    }

    // Sort windows by z-order
    window_t *sorted[MAX_WINDOWS];
    int count = 0;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i] && (windows[i]->flags & WINDOW_FLAG_VISIBLE)) {
            sorted[count++] = windows[i];
        }
    }

    // Simple bubble sort by z-order
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sorted[j]->z_order > sorted[j + 1]->z_order) {
                window_t *tmp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = tmp;
            }
        }
    }

    // Render windows bottom to top
    for (int i = 0; i < count; i++) {
        window_t *win = sorted[i];

        // Call paint callback if set
        if (win->on_paint) {
            win->on_paint(win);
        }

        int32_t wx = win->x;
        int32_t wy = win->y;

        // Draw border
        fill_rect(wx, wy, win->width, win->height, WM_COLOR_BORDER);

        // Draw title bar
        uint32_t title_color = (win->flags & WINDOW_FLAG_FOCUSED) ?
                               WM_COLOR_TITLE_ACTIVE : WM_COLOR_TITLE_INACTIVE;
        fill_rect(wx + 1, wy + 1, win->width - 2, WINDOW_TITLE_HEIGHT - 1, title_color);

        // Draw title text
        draw_string(win->title, wx + 4, wy + 6, WM_COLOR_TITLE_TEXT, title_color);

        // Draw close button if closable
        if (win->flags & WINDOW_FLAG_CLOSABLE) {
            int32_t btn_x = wx + win->width - WINDOW_TITLE_HEIGHT + 2;
            int32_t btn_y = wy + 2;
            fill_rect(btn_x, btn_y, WINDOW_TITLE_HEIGHT - 4, WINDOW_TITLE_HEIGHT - 4, WM_COLOR_CLOSE_BTN);
            // Draw X
            draw_string("X", btn_x + 4, btn_y + 4, WM_COLOR_TITLE_TEXT, WM_COLOR_CLOSE_BTN);
        }

        // Draw content area
        int32_t content_x = wx + WINDOW_BORDER_SIZE;
        int32_t content_y = wy + WINDOW_TITLE_HEIGHT;

        // Copy window framebuffer to screen
        if (win->framebuffer) {
            for (uint32_t py = 0; py < win->content_height; py++) {
                for (uint32_t px = 0; px < win->content_width; px++) {
                    int32_t screen_x = content_x + px;
                    int32_t screen_y = content_y + py;
                    if (screen_x >= 0 && screen_y >= 0 &&
                        screen_x < (int32_t)get_screen_width() &&
                        screen_y < (int32_t)get_screen_height()) {
                        put_pixel(screen_x, screen_y,
                                  win->framebuffer[py * win->content_width + px]);
                    }
                }
            }
        }

        win->flags &= ~WINDOW_FLAG_DIRTY;
    }

    // Clear global dirty flag
    wm_global_dirty = 0;

    // Show cursor again (saves fresh background from rendered windows)
    cursor_show();
}
