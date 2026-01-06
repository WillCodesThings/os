#include "gui/desktop.h"
#include "gui/window.h"
#include "gui/texteditor.h"
#include "gui/drawapp.h"
#include "gui/guiterminal.h"
#include "gui/bmp.h"
#include "gui/png.h"
#include "gui/jpg.h"
#include "graphics/graphics.h"
#include "graphics/font.h"
#include "memory/heap.h"
#include "utils/string.h"
#include "fs/vfs.h"

// Global desktop instance
static desktop_t g_desktop = {0};

// Forward declaration
static void render_taskbar(void);

// Screen dimensions (from graphics)
extern uint32_t get_screen_width(void);
extern uint32_t get_screen_height(void);

// Button labels
static const char *button_labels[] = {"Editor", "Draw", "Terminal"};
#define NUM_BUTTONS 3

// Helper: Draw text directly to screen
static void draw_taskbar_text(const char *text, int x, int y, uint32_t color) {
    while (*text) {
        char ch = *text;
        if (ch >= 32 && ch < 127) {
            for (int cy = 0; cy < 8; cy++) {
                for (int cx = 0; cx < 8; cx++) {
                    if (font_8x8[(int)ch][cy] & (1 << (7 - cx))) {
                        put_pixel(x + cx, y + cy, color);
                    }
                }
            }
        }
        x += 8;
        text++;
    }
}

// Get button rectangle
static void get_button_rect(int btn_index, int *x, int *y, int *w, int *h) {
    uint32_t screen_h = get_screen_height();
    *x = TASKBAR_PADDING + btn_index * (TASKBAR_BUTTON_WIDTH + TASKBAR_PADDING);
    *y = screen_h - TASKBAR_HEIGHT + (TASKBAR_HEIGHT - TASKBAR_BUTTON_HEIGHT) / 2;
    *w = TASKBAR_BUTTON_WIDTH;
    *h = TASKBAR_BUTTON_HEIGHT;
}

// Check if point is in button
static int point_in_button(int btn_index, int32_t mx, int32_t my) {
    int x, y, w, h;
    get_button_rect(btn_index, &x, &y, &w, &h);
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

// Initialize desktop
void desktop_init(void) {
    g_desktop.active = 1;
    g_desktop.editor = 0;
    g_desktop.drawapp = 0;
    g_desktop.terminal = 0;
    g_desktop.hover_button = -1;

    // Initialize window manager
    wm_init();

    // Draw initial desktop
    desktop_render();
}

// Shutdown desktop
void desktop_shutdown(void) {
    // Close all apps
    if (g_desktop.editor) {
        texteditor_close(g_desktop.editor);
        g_desktop.editor = 0;
    }
    if (g_desktop.drawapp) {
        drawapp_close(g_desktop.drawapp);
        g_desktop.drawapp = 0;
    }
    if (g_desktop.terminal) {
        guiterminal_close(g_desktop.terminal);
        g_desktop.terminal = 0;
    }

    g_desktop.active = 0;
}

// Check if desktop is active
int desktop_is_active(void) {
    return g_desktop.active;
}

// Get focused app type
app_type_t desktop_get_focused_app(void) {
    window_t *focused = wm_get_focused_window();
    if (!focused) return APP_NONE;

    if (g_desktop.editor && g_desktop.editor->window == focused) {
        return APP_EDITOR;
    }
    if (g_desktop.drawapp && g_desktop.drawapp->window == focused) {
        return APP_DRAW;
    }
    if (g_desktop.terminal && g_desktop.terminal->window == focused) {
        return APP_TERMINAL;
    }

    return APP_NONE;
}

// Handle keyboard
int desktop_handle_key(char c) {
    if (!g_desktop.active) return 0;

    app_type_t focused = desktop_get_focused_app();

    switch (focused) {
        case APP_EDITOR:
            if (g_desktop.editor) {
                texteditor_handle_key(g_desktop.editor, c);
                return 1;
            }
            break;
        case APP_TERMINAL:
            if (g_desktop.terminal) {
                guiterminal_handle_key(g_desktop.terminal, c);
                return 1;
            }
            break;
        case APP_DRAW:
            // Draw app doesn't use keyboard
            return 0;
        default:
            break;
    }

    return 0;
}

// Handle mouse
int desktop_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    if (!g_desktop.active) return 0;

    uint32_t screen_h = get_screen_height();
    static uint8_t last_buttons = 0;
    uint8_t left_click = (buttons & 0x01) && !(last_buttons & 0x01);

    // Check if in taskbar area
    if (y >= (int32_t)(screen_h - TASKBAR_HEIGHT)) {
        // Update hover state
        int new_hover = -1;
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (point_in_button(i, x, y)) {
                new_hover = i;
                break;
            }
        }

        if (new_hover != g_desktop.hover_button) {
            g_desktop.hover_button = new_hover;
            render_taskbar();  // Only redraw taskbar, not whole desktop
        }

        // Handle click
        if (left_click && new_hover >= 0) {
            switch (new_hover) {
                case 0: desktop_open_app(APP_EDITOR); break;
                case 1: desktop_open_app(APP_DRAW); break;
                case 2: desktop_open_app(APP_TERMINAL); break;
            }
        }

        last_buttons = buttons;
        return 1;  // Consumed by taskbar
    }

    // Clear hover when not in taskbar
    if (g_desktop.hover_button != -1) {
        g_desktop.hover_button = -1;
        render_taskbar();  // Only redraw taskbar
    }

    // Route mouse to draw app if mouse is within its content area
    if (g_desktop.drawapp && g_desktop.drawapp->window) {
        window_t *win = g_desktop.drawapp->window;
        int32_t content_x = win->x + WINDOW_BORDER_SIZE;
        int32_t content_y = win->y + WINDOW_TITLE_HEIGHT;
        int32_t content_w = win->content_width;
        int32_t content_h = win->content_height;

        // Check if mouse is in draw app content area (not title bar)
        if (x >= content_x && x < content_x + content_w &&
            y >= content_y && y < content_y + content_h) {
            drawapp_handle_mouse(g_desktop.drawapp, x, y, buttons);
        }
    }

    last_buttons = buttons;
    return 0;  // Let window manager handle it
}

// Render just the taskbar (doesn't redraw desktop background)
static void render_taskbar(void) {
    uint32_t screen_w = get_screen_width();
    uint32_t screen_h = get_screen_height();

    // Draw taskbar background
    int taskbar_y = screen_h - TASKBAR_HEIGHT;
    fill_rect(0, taskbar_y, screen_w, TASKBAR_HEIGHT, TASKBAR_BG_COLOR);

    // Draw top border on taskbar
    fill_rect(0, taskbar_y, screen_w, 1, 0x404040);

    // Draw buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
        int bx, by, bw, bh;
        get_button_rect(i, &bx, &by, &bw, &bh);

        // Button background (highlight if hovered or app is open)
        uint32_t btn_color = TASKBAR_BTN_COLOR;
        if (i == g_desktop.hover_button) {
            btn_color = TASKBAR_BTN_HOVER;
        }

        // Check if app is open (draw indicator)
        int app_open = 0;
        if (i == 0 && g_desktop.editor) app_open = 1;
        if (i == 1 && g_desktop.drawapp) app_open = 1;
        if (i == 2 && g_desktop.terminal) app_open = 1;

        // Draw button
        fill_rect(bx, by, bw, bh, btn_color);

        // Draw border
        fill_rect(bx, by, bw, 1, 0x606060);  // Top
        fill_rect(bx, by, 1, bh, 0x606060);  // Left
        fill_rect(bx, by + bh - 1, bw, 1, 0x202020);  // Bottom
        fill_rect(bx + bw - 1, by, 1, bh, 0x202020);  // Right

        // Draw label centered
        const char *label = button_labels[i];
        int label_len = strlen(label);
        int text_x = bx + (bw - label_len * 8) / 2;
        int text_y = by + (bh - 8) / 2;
        draw_taskbar_text(label, text_x, text_y, TASKBAR_BTN_TEXT);

        // Draw indicator dot if app is open
        if (app_open) {
            int dot_x = bx + bw / 2 - 2;
            int dot_y = by + bh + 2;
            fill_rect(dot_x, dot_y, 4, 2, 0x00AAFF);
        }
    }
}

// Render full desktop (background + taskbar)
void desktop_render(void) {
    if (!g_desktop.active) return;

    uint32_t screen_w = get_screen_width();
    uint32_t screen_h = get_screen_height();

    // Draw desktop background
    if (g_desktop.bg_pixels && g_desktop.bg_width > 0 && g_desktop.bg_height > 0) {
        // Draw background image (scaled/tiled to fit)
        uint32_t desktop_h = screen_h - TASKBAR_HEIGHT;
        for (uint32_t y = 0; y < desktop_h; y++) {
            uint32_t src_y = (y * g_desktop.bg_height) / desktop_h;
            if (src_y >= g_desktop.bg_height) src_y = g_desktop.bg_height - 1;
            for (uint32_t x = 0; x < screen_w; x++) {
                uint32_t src_x = (x * g_desktop.bg_width) / screen_w;
                if (src_x >= g_desktop.bg_width) src_x = g_desktop.bg_width - 1;
                put_pixel(x, y, g_desktop.bg_pixels[src_y * g_desktop.bg_width + src_x]);
            }
        }
    } else {
        // Draw solid teal color background
        fill_rect(0, 0, screen_w, screen_h - TASKBAR_HEIGHT, DESKTOP_BG_COLOR);
    }

    // Draw taskbar
    render_taskbar();
}

// Open an application
void desktop_open_app(app_type_t app) {
    switch (app) {
        case APP_EDITOR:
            if (!g_desktop.editor) {
                g_desktop.editor = texteditor_create("Text Editor", 50, 50);
            } else {
                // Focus existing window
                window_focus(g_desktop.editor->window);
            }
            break;

        case APP_DRAW:
            if (!g_desktop.drawapp) {
                g_desktop.drawapp = drawapp_create("Draw", 150, 80);
            } else {
                window_focus(g_desktop.drawapp->window);
            }
            break;

        case APP_TERMINAL:
            if (!g_desktop.terminal) {
                g_desktop.terminal = guiterminal_create("Terminal", 250, 120);
            } else {
                window_focus(g_desktop.terminal->window);
            }
            break;

        default:
            break;
    }

    // Redraw taskbar to show open indicator
    desktop_render();
    wm_force_render();
}

// Called when a window is destroyed - clear our reference
void desktop_notify_window_closed(window_t *win) {
    if (!g_desktop.active) return;

    if (g_desktop.editor && g_desktop.editor->window == win) {
        g_desktop.editor = 0;
    }
    if (g_desktop.drawapp && g_desktop.drawapp->window == win) {
        g_desktop.drawapp = 0;
    }
    if (g_desktop.terminal && g_desktop.terminal->window == win) {
        g_desktop.terminal = 0;
    }
    render_taskbar();  // Update indicator dots
}

// Detect image format from data
static int detect_image_format(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    // Check BMP signature (BM)
    if (data[0] == 'B' && data[1] == 'M') return 1;  // BMP

    // Check PNG signature
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return 2;  // PNG

    // Check JPEG signature (FFD8)
    if (data[0] == 0xFF && data[1] == 0xD8) return 3;  // JPG

    return 0;  // Unknown
}

// Set desktop background image from file
int desktop_set_background(const char *path) {
    if (!path) return -1;

    // Read file
    vfs_node_t *file = vfs_open(path, VFS_READ);
    if (!file) return -1;

    uint8_t *buffer = (uint8_t *)kmalloc(file->length);
    if (!buffer) {
        vfs_close(file);
        return -1;
    }

    int bytes_read = vfs_read(file, 0, file->length, buffer);
    vfs_close(file);

    if (bytes_read <= 0) {
        kfree(buffer);
        return -1;
    }

    // Detect format and load
    int format = detect_image_format(buffer, bytes_read);
    uint32_t *pixels = 0;
    uint32_t width = 0, height = 0;

    if (format == 1) {
        // BMP
        bmp_image_t *img = bmp_load(buffer, bytes_read);
        if (img) {
            pixels = img->pixels;
            width = img->width;
            height = img->height;
            kfree(img);  // Free struct but keep pixels
        }
    } else if (format == 2) {
        // PNG
        png_image_t *img = png_load(buffer, bytes_read);
        if (img) {
            pixels = img->pixels;
            width = img->width;
            height = img->height;
            kfree(img);  // Free struct but keep pixels
        }
    } else if (format == 3) {
        // JPG
        jpg_image_t *img = jpg_load(buffer, bytes_read);
        if (img) {
            pixels = img->pixels;
            width = img->width;
            height = img->height;
            kfree(img);  // Free struct but keep pixels
        }
    }

    kfree(buffer);

    if (!pixels) return -1;

    // Free old background if any
    if (g_desktop.bg_pixels) {
        kfree(g_desktop.bg_pixels);
    }

    // Set new background
    g_desktop.bg_pixels = pixels;
    g_desktop.bg_width = width;
    g_desktop.bg_height = height;

    // Redraw desktop
    desktop_render();
    wm_force_render();

    return 0;
}
