#pragma once
#include <stdint.h>
#include <stddef.h>

// PNG image structure
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;  // ARGB format
} png_image_t;

// Load PNG from memory buffer
png_image_t *png_load(const uint8_t *data, size_t size);

// Load PNG from file
png_image_t *png_load_file(const char *path);

// Free PNG image
void png_free(png_image_t *img);
