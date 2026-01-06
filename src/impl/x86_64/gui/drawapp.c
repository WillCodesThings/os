#include "gui/drawapp.h"
#include "gui/window.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "graphics/font.h"

// Colors for palette
static const uint32_t palette_colors[] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00,
    0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
    0x808080, 0xC0C0C0, 0x800000, 0x008000,
    0x000080, 0x808000, 0x800080, 0x008080
};
#define PALETTE_COUNT 16
#define PALETTE_SIZE 16

// Tool icons (simple representations)
static const char *tool_names[] = {"P", "B", "L", "R", "C", "F", "E"};
#define TOOL_COUNT 7
#define TOOL_BTN_SIZE 20

// Paint callback
static void drawapp_paint(window_t *win) {
    if (!win) return;
    draw_app_t *app = (draw_app_t *)win->user_data;
    if (!app || !app->canvas) return;

    // Clear to gray background
    window_clear(win, 0x808080);

    // Draw toolbar background
    window_fill_rect(win, 0, 0, win->content_width, DRAWAPP_TOOLBAR_HEIGHT, 0xC0C0C0);

    // Draw tool buttons
    for (int i = 0; i < TOOL_COUNT; i++) {
        int btn_x = 4 + i * (TOOL_BTN_SIZE + 2);
        int btn_y = 2;

        // Button background
        uint32_t btn_color = (i == (int)app->current_tool) ? 0x6060FF : 0xE0E0E0;
        window_fill_rect(win, btn_x, btn_y, TOOL_BTN_SIZE, TOOL_BTN_SIZE, btn_color);

        // Button border
        window_fill_rect(win, btn_x, btn_y, TOOL_BTN_SIZE, 1, 0x000000);
        window_fill_rect(win, btn_x, btn_y + TOOL_BTN_SIZE - 1, TOOL_BTN_SIZE, 1, 0x000000);
        window_fill_rect(win, btn_x, btn_y, 1, TOOL_BTN_SIZE, 0x000000);
        window_fill_rect(win, btn_x + TOOL_BTN_SIZE - 1, btn_y, 1, TOOL_BTN_SIZE, 0x000000);

        // Tool letter
        char ch = tool_names[i][0];
        uint32_t text_color = (i == (int)app->current_tool) ? 0xFFFFFF : 0x000000;
        for (int cy = 0; cy < 8; cy++) {
            for (int cx = 0; cx < 8; cx++) {
                if (font_8x8[(int)ch][cy] & (1 << (7 - cx))) {
                    window_put_pixel(win, btn_x + 6 + cx, btn_y + 6 + cy, text_color);
                }
            }
        }
    }

    // Draw color palette
    int palette_x = 4 + TOOL_COUNT * (TOOL_BTN_SIZE + 2) + 10;
    for (int i = 0; i < PALETTE_COUNT; i++) {
        int px = palette_x + (i % 8) * (PALETTE_SIZE + 1);
        int py = 2 + (i / 8) * (PALETTE_SIZE / 2 + 1);

        // Color swatch
        window_fill_rect(win, px, py, PALETTE_SIZE, PALETTE_SIZE / 2, palette_colors[i]);

        // Border for selected color
        if (palette_colors[i] == app->current_color) {
            window_fill_rect(win, px - 1, py - 1, PALETTE_SIZE + 2, 1, 0xFFFF00);
            window_fill_rect(win, px - 1, py + PALETTE_SIZE / 2, PALETTE_SIZE + 2, 1, 0xFFFF00);
            window_fill_rect(win, px - 1, py - 1, 1, PALETTE_SIZE / 2 + 2, 0xFFFF00);
            window_fill_rect(win, px + PALETTE_SIZE, py - 1, 1, PALETTE_SIZE / 2 + 2, 0xFFFF00);
        }
    }

    // Draw current color preview
    int preview_x = win->content_width - 30;
    window_fill_rect(win, preview_x, 4, 24, 16, app->current_color);
    window_fill_rect(win, preview_x, 4, 24, 1, 0x000000);
    window_fill_rect(win, preview_x, 19, 24, 1, 0x000000);
    window_fill_rect(win, preview_x, 4, 1, 16, 0x000000);
    window_fill_rect(win, preview_x + 23, 4, 1, 16, 0x000000);

    // Draw canvas border
    int canvas_x = 2;
    int canvas_y = DRAWAPP_TOOLBAR_HEIGHT + 2;
    window_fill_rect(win, canvas_x - 1, canvas_y - 1,
                    app->canvas_width + 2, app->canvas_height + 2, 0x000000);

    // Draw canvas content
    if (app->canvas) {
        for (uint32_t y = 0; y < app->canvas_height; y++) {
            for (uint32_t x = 0; x < app->canvas_width; x++) {
                window_put_pixel(win, canvas_x + x, canvas_y + y,
                               app->canvas[y * app->canvas_width + x]);
            }
        }
    }
}

// Close callback
static void drawapp_on_close(window_t *win) {
    draw_app_t *app = (draw_app_t *)win->user_data;
    if (app) {
        if (app->canvas) {
            kfree(app->canvas);
        }
        kfree(app);
    }
}

// Helper: Draw line on canvas using Bresenham's algorithm
static void canvas_draw_line(draw_app_t *app, int x0, int y0, int x1, int y1, uint32_t color) {
    if (!app || !app->canvas) return;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    dx = dx > 0 ? dx : -dx;
    dy = dy > 0 ? dy : -dy;

    int err = dx - dy;

    while (1) {
        if (x0 >= 0 && x0 < (int)app->canvas_width &&
            y0 >= 0 && y0 < (int)app->canvas_height) {
            app->canvas[y0 * app->canvas_width + x0] = color;

            // For brush, draw thicker
            if (app->current_tool == TOOL_BRUSH && app->brush_size > 1) {
                for (int by = -app->brush_size/2; by <= app->brush_size/2; by++) {
                    for (int bx = -app->brush_size/2; bx <= app->brush_size/2; bx++) {
                        int px = x0 + bx;
                        int py = y0 + by;
                        if (px >= 0 && px < (int)app->canvas_width &&
                            py >= 0 && py < (int)app->canvas_height) {
                            app->canvas[py * app->canvas_width + px] = color;
                        }
                    }
                }
            }
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

// Helper: Draw rectangle on canvas
static void canvas_draw_rect(draw_app_t *app, int x0, int y0, int x1, int y1, uint32_t color) {
    if (!app || !app->canvas) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }

    for (int x = x0; x <= x1; x++) {
        if (x >= 0 && x < (int)app->canvas_width) {
            if (y0 >= 0 && y0 < (int)app->canvas_height)
                app->canvas[y0 * app->canvas_width + x] = color;
            if (y1 >= 0 && y1 < (int)app->canvas_height)
                app->canvas[y1 * app->canvas_width + x] = color;
        }
    }
    for (int y = y0; y <= y1; y++) {
        if (y >= 0 && y < (int)app->canvas_height) {
            if (x0 >= 0 && x0 < (int)app->canvas_width)
                app->canvas[y * app->canvas_width + x0] = color;
            if (x1 >= 0 && x1 < (int)app->canvas_width)
                app->canvas[y * app->canvas_width + x1] = color;
        }
    }
}

// Helper: Draw circle on canvas
static void canvas_draw_circle(draw_app_t *app, int cx, int cy, int radius, uint32_t color) {
    if (!app || !app->canvas) return;
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        // Draw 8 octants
        int points[][2] = {
            {cx + x, cy + y}, {cx + y, cy + x},
            {cx - y, cy + x}, {cx - x, cy + y},
            {cx - x, cy - y}, {cx - y, cy - x},
            {cx + y, cy - x}, {cx + x, cy - y}
        };

        for (int i = 0; i < 8; i++) {
            int px = points[i][0];
            int py = points[i][1];
            if (px >= 0 && px < (int)app->canvas_width &&
                py >= 0 && py < (int)app->canvas_height) {
                app->canvas[py * app->canvas_width + px] = color;
            }
        }

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

// Helper: Flood fill on canvas using scanline algorithm (more stack-efficient)
static void canvas_flood_fill(draw_app_t *app, int x, int y, uint32_t new_color) {
    if (!app || !app->canvas) return;
    if (x < 0 || x >= (int)app->canvas_width ||
        y < 0 || y >= (int)app->canvas_height) return;

    uint32_t old_color = app->canvas[y * app->canvas_width + x];
    if (old_color == new_color) return;

    // Use heap allocation for fill stack to avoid kernel stack overflow
    #define FILL_STACK_SIZE 256
    int16_t *stack_x = (int16_t *)kmalloc(FILL_STACK_SIZE * sizeof(int16_t));
    int16_t *stack_y = (int16_t *)kmalloc(FILL_STACK_SIZE * sizeof(int16_t));

    if (!stack_x || !stack_y) {
        if (stack_x) kfree(stack_x);
        if (stack_y) kfree(stack_y);
        return;
    }

    int stack_pos = 0;

    stack_x[stack_pos] = (int16_t)x;
    stack_y[stack_pos] = (int16_t)y;
    stack_pos++;

    while (stack_pos > 0 && stack_pos < FILL_STACK_SIZE - 4) {
        stack_pos--;
        int px = stack_x[stack_pos];
        int py = stack_y[stack_pos];

        if (px < 0 || px >= (int)app->canvas_width ||
            py < 0 || py >= (int)app->canvas_height) continue;

        if (app->canvas[py * app->canvas_width + px] != old_color) continue;

        app->canvas[py * app->canvas_width + px] = new_color;

        // Push neighbors
        stack_x[stack_pos] = (int16_t)(px + 1); stack_y[stack_pos] = (int16_t)py; stack_pos++;
        stack_x[stack_pos] = (int16_t)(px - 1); stack_y[stack_pos] = (int16_t)py; stack_pos++;
        stack_x[stack_pos] = (int16_t)px; stack_y[stack_pos] = (int16_t)(py + 1); stack_pos++;
        stack_x[stack_pos] = (int16_t)px; stack_y[stack_pos] = (int16_t)(py - 1); stack_pos++;
    }

    kfree(stack_x);
    kfree(stack_y);
}

// Create drawing app
draw_app_t *drawapp_create(const char *title, int32_t x, int32_t y) {
    draw_app_t *app = (draw_app_t *)kcalloc(1, sizeof(draw_app_t));
    if (!app) return 0;

    // Create window
    app->window = window_create(title, x, y,
                                DRAWAPP_WIDTH, DRAWAPP_HEIGHT,
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE |
                                WINDOW_FLAG_CLOSABLE);

    if (!app->window) {
        kfree(app);
        return 0;
    }

    // Set callbacks and user data
    app->window->user_data = app;
    app->window->on_paint = drawapp_paint;
    app->window->on_close = drawapp_on_close;

    // Initialize canvas
    app->canvas_width = DRAWAPP_WIDTH - 4;
    app->canvas_height = DRAWAPP_HEIGHT - DRAWAPP_TOOLBAR_HEIGHT - 4;
    app->canvas = (uint32_t *)kmalloc(app->canvas_width * app->canvas_height * sizeof(uint32_t));

    if (!app->canvas) {
        window_destroy(app->window);
        kfree(app);
        return 0;
    }

    // Clear canvas to white
    for (uint32_t i = 0; i < app->canvas_width * app->canvas_height; i++) {
        app->canvas[i] = 0xFFFFFF;
    }

    // Initialize state
    app->current_tool = TOOL_PENCIL;
    app->current_color = 0x000000;
    app->brush_size = 3;
    app->is_drawing = 0;

    return app;
}

// Handle mouse input (coordinates relative to window content area)
void drawapp_handle_mouse(draw_app_t *app, int32_t x, int32_t y, uint8_t buttons) {
    if (!app || !app->window || !app->canvas) return;

    int left_pressed = buttons & 0x01;

    // Adjust for window position
    x -= app->window->x + WINDOW_BORDER_SIZE;
    y -= app->window->y + WINDOW_TITLE_HEIGHT;

    // Safety bounds check for content area
    if (x < 0 || x >= (int32_t)app->window->content_width ||
        y < 0 || y >= (int32_t)app->window->content_height) {
        if (!left_pressed) app->is_drawing = 0;
        return;
    }

    // Check if in toolbar area
    if (y < DRAWAPP_TOOLBAR_HEIGHT && left_pressed && !app->is_drawing) {
        // Check tool buttons
        for (int i = 0; i < TOOL_COUNT; i++) {
            int btn_x = 4 + i * (TOOL_BTN_SIZE + 2);
            if (x >= btn_x && x < btn_x + TOOL_BTN_SIZE &&
                y >= 2 && y < 2 + TOOL_BTN_SIZE) {
                app->current_tool = (draw_tool_t)i;
                window_invalidate(app->window);
                return;
            }
        }

        // Check palette
        int palette_x = 4 + TOOL_COUNT * (TOOL_BTN_SIZE + 2) + 10;
        for (int i = 0; i < PALETTE_COUNT; i++) {
            int px = palette_x + (i % 8) * (PALETTE_SIZE + 1);
            int py = 2 + (i / 8) * (PALETTE_SIZE / 2 + 1);
            if (x >= px && x < px + PALETTE_SIZE &&
                y >= py && y < py + PALETTE_SIZE / 2) {
                app->current_color = palette_colors[i];
                window_invalidate(app->window);
                return;
            }
        }
        return;
    }

    // Convert to canvas coordinates
    int canvas_x = x - 2;
    int canvas_y = y - DRAWAPP_TOOLBAR_HEIGHT - 2;

    if (canvas_x < 0 || canvas_x >= (int)app->canvas_width ||
        canvas_y < 0 || canvas_y >= (int)app->canvas_height) {
        if (!left_pressed) app->is_drawing = 0;
        return;
    }

    uint32_t draw_color = (app->current_tool == TOOL_ERASER) ? 0xFFFFFF : app->current_color;

    if (left_pressed) {
        if (!app->is_drawing) {
            // Start drawing
            app->is_drawing = 1;
            app->start_x = canvas_x;
            app->start_y = canvas_y;
            app->last_x = canvas_x;
            app->last_y = canvas_y;

            // Handle single-click tools
            switch (app->current_tool) {
                case TOOL_FILL:
                    canvas_flood_fill(app, canvas_x, canvas_y, draw_color);
                    window_invalidate(app->window);
                    break;
                case TOOL_PENCIL:
                case TOOL_BRUSH:
                case TOOL_ERASER:
                    app->canvas[canvas_y * app->canvas_width + canvas_x] = draw_color;
                    window_invalidate(app->window);
                    break;
                default:
                    break;
            }
        } else {
            // Continue drawing
            switch (app->current_tool) {
                case TOOL_PENCIL:
                case TOOL_BRUSH:
                case TOOL_ERASER:
                    canvas_draw_line(app, app->last_x, app->last_y, canvas_x, canvas_y, draw_color);
                    app->last_x = canvas_x;
                    app->last_y = canvas_y;
                    window_invalidate(app->window);
                    break;
                default:
                    // For shapes, we'll draw on release
                    break;
            }
        }
    } else {
        // Button released - finalize shapes
        if (app->is_drawing) {
            switch (app->current_tool) {
                case TOOL_LINE:
                    canvas_draw_line(app, app->start_x, app->start_y, canvas_x, canvas_y, draw_color);
                    window_invalidate(app->window);
                    break;
                case TOOL_RECT:
                    canvas_draw_rect(app, app->start_x, app->start_y, canvas_x, canvas_y, draw_color);
                    window_invalidate(app->window);
                    break;
                case TOOL_CIRCLE: {
                    int dx = canvas_x - app->start_x;
                    int dy = canvas_y - app->start_y;
                    int radius = dx * dx + dy * dy;
                    // Simple integer square root
                    int r = 0;
                    while (r * r < radius) r++;
                    canvas_draw_circle(app, app->start_x, app->start_y, r, draw_color);
                    window_invalidate(app->window);
                    break;
                }
                default:
                    break;
            }
            app->is_drawing = 0;
        }
    }
}

// Set current tool
void drawapp_set_tool(draw_app_t *app, draw_tool_t tool) {
    if (app) {
        app->current_tool = tool;
        window_invalidate(app->window);
    }
}

// Set current color
void drawapp_set_color(draw_app_t *app, uint32_t color) {
    if (app) {
        app->current_color = color;
        window_invalidate(app->window);
    }
}

// Clear canvas
void drawapp_clear(draw_app_t *app) {
    if (app && app->canvas) {
        for (uint32_t i = 0; i < app->canvas_width * app->canvas_height; i++) {
            app->canvas[i] = 0xFFFFFF;
        }
        window_invalidate(app->window);
    }
}

// Close drawing app
void drawapp_close(draw_app_t *app) {
    if (app && app->window) {
        window_destroy(app->window);
    }
}

// Refresh display
void drawapp_refresh(draw_app_t *app) {
    if (app && app->window) {
        window_invalidate(app->window);
    }
}
