#pragma once
#include <stdint.h>
#include <stddef.h>

// BMP file header
typedef struct {
    uint16_t signature;      // 'BM'
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;    // Offset to pixel data
} __attribute__((packed)) bmp_file_header_t;

// BMP info header (BITMAPINFOHEADER)
typedef struct {
    uint32_t header_size;    // Size of this header (40)
    int32_t width;
    int32_t height;          // Negative = top-down
    uint16_t planes;         // Must be 1
    uint16_t bits_per_pixel; // 24 or 32
    uint32_t compression;    // 0 = none
    uint32_t image_size;
    int32_t x_pixels_per_m;
    int32_t y_pixels_per_m;
    uint32_t colors_used;
    uint32_t important_colors;
} __attribute__((packed)) bmp_info_header_t;

// Loaded BMP image
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;        // ARGB format
} bmp_image_t;

// BMP functions
int bmp_validate(const uint8_t *data, size_t size);
bmp_image_t *bmp_load(const uint8_t *data, size_t size);
bmp_image_t *bmp_load_file(const char *path);
void bmp_free(bmp_image_t *image);

// Get image dimensions without loading
int bmp_get_dimensions(const uint8_t *data, size_t size,
                       uint32_t *width, uint32_t *height);
