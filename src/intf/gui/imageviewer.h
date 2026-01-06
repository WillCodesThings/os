#pragma once
#include <stdint.h>
#include "gui/window.h"
#include "gui/bmp.h"

// Generic image type (same layout as bmp_image_t, png_image_t, jpg_image_t)
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;  // ARGB format
} generic_image_t;

// Image format types
typedef enum {
    IMG_FORMAT_UNKNOWN = 0,
    IMG_FORMAT_BMP,
    IMG_FORMAT_PNG,
    IMG_FORMAT_JPG
} image_format_t;

// Image viewer structure
typedef struct {
    window_t *window;
    generic_image_t *image;  // Use generic image type
    image_format_t format;
    uint32_t scroll_x;
    uint32_t scroll_y;
    uint32_t zoom_percent;  // 100 = 100% zoom (no float - FPU not enabled)
} image_viewer_t;

// Create image viewer window
image_viewer_t *imageviewer_create(const char *title, int32_t x, int32_t y);

// Load image from file
int imageviewer_load_file(image_viewer_t *viewer, const char *path);

// Load image from memory
int imageviewer_load_data(image_viewer_t *viewer, const uint8_t *data, size_t size);

// Close image viewer
void imageviewer_close(image_viewer_t *viewer);

// Redraw the viewer
void imageviewer_refresh(image_viewer_t *viewer);
