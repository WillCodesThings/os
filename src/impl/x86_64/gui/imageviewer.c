#include "gui/imageviewer.h"
#include "gui/window.h"
#include "gui/bmp.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "utils/string.h"

// Default viewer size
#define VIEWER_DEFAULT_WIDTH  400
#define VIEWER_DEFAULT_HEIGHT 300

// Paint callback
static void imageviewer_paint(window_t *win) {
    image_viewer_t *viewer = (image_viewer_t *)win->user_data;
    if (!viewer || !viewer->image) {
        // Draw placeholder
        window_clear(win, 0x333333);
        return;
    }

    // Clear background
    window_clear(win, 0x222222);

    // Draw image
    bmp_image_t *img = viewer->image;

    // Calculate centered position if image is smaller than window
    int32_t offset_x = 0;
    int32_t offset_y = 0;

    if (img->width < win->content_width) {
        offset_x = (win->content_width - img->width) / 2;
    }
    if (img->height < win->content_height) {
        offset_y = (win->content_height - img->height) / 2;
    }

    // Draw image pixels
    for (uint32_t y = 0; y < img->height && y + offset_y < win->content_height; y++) {
        for (uint32_t x = 0; x < img->width && x + offset_x < win->content_width; x++) {
            if (x + offset_x >= 0 && y + offset_y >= 0) {
                window_put_pixel(win, offset_x + x, offset_y + y,
                                img->pixels[y * img->width + x]);
            }
        }
    }
}

// Close callback
static void imageviewer_on_close(window_t *win) {
    image_viewer_t *viewer = (image_viewer_t *)win->user_data;
    if (viewer) {
        if (viewer->image) {
            bmp_free(viewer->image);
        }
        kfree(viewer);
    }
}

// Create image viewer
image_viewer_t *imageviewer_create(const char *title, int32_t x, int32_t y) {
    image_viewer_t *viewer = (image_viewer_t *)kcalloc(1, sizeof(image_viewer_t));
    if (!viewer) return 0;

    // Create window
    viewer->window = window_create(title, x, y,
                                   VIEWER_DEFAULT_WIDTH, VIEWER_DEFAULT_HEIGHT,
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE |
                                   WINDOW_FLAG_CLOSABLE);

    if (!viewer->window) {
        kfree(viewer);
        return 0;
    }

    // Set callbacks and user data
    viewer->window->user_data = viewer;
    viewer->window->on_paint = imageviewer_paint;
    viewer->window->on_close = imageviewer_on_close;
    viewer->zoom_percent = 100;  // 100% zoom

    return viewer;
}

// Load from file
int imageviewer_load_file(image_viewer_t *viewer, const char *path) {
    if (!viewer) return -1;

    // Free existing image
    if (viewer->image) {
        bmp_free(viewer->image);
        viewer->image = 0;
    }

    // Load new image
    viewer->image = bmp_load_file(path);
    if (!viewer->image) {
        return -1;
    }

    // Update window title
    const char *filename = path;
    // Find last / or start
    for (const char *p = path; *p; p++) {
        if (*p == '/') filename = p + 1;
    }
    window_set_title(viewer->window, filename);

    // Resize window to fit image (with limits)
    uint32_t new_width = viewer->image->width;
    uint32_t new_height = viewer->image->height;

    if (new_width > 800) new_width = 800;
    if (new_height > 600) new_height = 600;
    if (new_width < 100) new_width = 100;
    if (new_height < 100) new_height = 100;

    window_resize(viewer->window, new_width, new_height);

    // Trigger repaint
    window_invalidate(viewer->window);

    return 0;
}

// Load from memory
int imageviewer_load_data(image_viewer_t *viewer, const uint8_t *data, size_t size) {
    if (!viewer || !data) return -1;

    // Free existing image
    if (viewer->image) {
        bmp_free(viewer->image);
        viewer->image = 0;
    }

    // Load new image
    viewer->image = bmp_load(data, size);
    if (!viewer->image) {
        return -1;
    }

    // Resize window to fit image (with limits)
    uint32_t new_width = viewer->image->width;
    uint32_t new_height = viewer->image->height;

    if (new_width > 800) new_width = 800;
    if (new_height > 600) new_height = 600;
    if (new_width < 100) new_width = 100;
    if (new_height < 100) new_height = 100;

    window_resize(viewer->window, new_width, new_height);

    // Trigger repaint
    window_invalidate(viewer->window);

    return 0;
}

// Close viewer
void imageviewer_close(image_viewer_t *viewer) {
    if (viewer && viewer->window) {
        window_destroy(viewer->window);
        // Note: window_destroy calls on_close which frees viewer
    }
}

// Refresh display
void imageviewer_refresh(image_viewer_t *viewer) {
    if (viewer && viewer->window) {
        window_invalidate(viewer->window);
    }
}
