#pragma once
#include <stdint.h>
#include <stddef.h>

// JPG image structure
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;  // ARGB format
} jpg_image_t;

// Load JPG from memory buffer
jpg_image_t *jpg_load(const uint8_t *data, size_t size);

// Load JPG from file
jpg_image_t *jpg_load_file(const char *path);

// Free JPG image
void jpg_free(jpg_image_t *img);
