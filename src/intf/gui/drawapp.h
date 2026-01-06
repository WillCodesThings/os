#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gui/window.h"

// Drawing app constants
#define DRAWAPP_WIDTH 320
#define DRAWAPP_HEIGHT 240
#define DRAWAPP_TOOLBAR_HEIGHT 24

// Drawing tools
typedef enum {
    TOOL_PENCIL,
    TOOL_BRUSH,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_CIRCLE,
    TOOL_FILL,
    TOOL_ERASER
} draw_tool_t;

// Drawing app structure
typedef struct draw_app {
    window_t *window;
    uint32_t *canvas;           // Drawing canvas pixels
    uint32_t canvas_width;
    uint32_t canvas_height;
    draw_tool_t current_tool;
    uint32_t current_color;
    int brush_size;
    int is_drawing;
    int last_x, last_y;         // For line drawing
    int start_x, start_y;       // For shapes
} draw_app_t;

// Create a new drawing app window
draw_app_t *drawapp_create(const char *title, int32_t x, int32_t y);

// Close the drawing app
void drawapp_close(draw_app_t *app);

// Handle mouse input
void drawapp_handle_mouse(draw_app_t *app, int32_t x, int32_t y, uint8_t buttons);

// Set current tool
void drawapp_set_tool(draw_app_t *app, draw_tool_t tool);

// Set current color
void drawapp_set_color(draw_app_t *app, uint32_t color);

// Clear canvas
void drawapp_clear(draw_app_t *app);

// Refresh the display
void drawapp_refresh(draw_app_t *app);
