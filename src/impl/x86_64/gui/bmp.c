#include "gui/bmp.h"
#include "memory/heap.h"
#include "fs/vfs.h"
#include "utils/memory.h"

// BMP signature
#define BMP_SIGNATURE 0x4D42  // 'BM' in little-endian

// Validate BMP file
int bmp_validate(const uint8_t *data, size_t size) {
    if (size < sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t)) {
        return -1;  // Too small
    }

    const bmp_file_header_t *file_header = (const bmp_file_header_t *)data;

    if (file_header->signature != BMP_SIGNATURE) {
        return -2;  // Invalid signature
    }

    const bmp_info_header_t *info_header = (const bmp_info_header_t *)(data + sizeof(bmp_file_header_t));

    // Check for supported formats
    if (info_header->bits_per_pixel != 24 && info_header->bits_per_pixel != 32) {
        return -3;  // Unsupported bit depth
    }

    if (info_header->compression != 0) {
        return -4;  // Compressed BMPs not supported
    }

    return 0;
}

// Get dimensions without loading
int bmp_get_dimensions(const uint8_t *data, size_t size,
                       uint32_t *width, uint32_t *height) {
    if (bmp_validate(data, size) != 0) {
        return -1;
    }

    const bmp_info_header_t *info = (const bmp_info_header_t *)(data + sizeof(bmp_file_header_t));

    *width = (info->width < 0) ? -info->width : info->width;
    *height = (info->height < 0) ? -info->height : info->height;

    return 0;
}

// Load BMP from memory
bmp_image_t *bmp_load(const uint8_t *data, size_t size) {
    if (bmp_validate(data, size) != 0) {
        return 0;
    }

    const bmp_file_header_t *file_header = (const bmp_file_header_t *)data;
    const bmp_info_header_t *info_header = (const bmp_info_header_t *)(data + sizeof(bmp_file_header_t));

    // Get dimensions
    int32_t width = info_header->width;
    int32_t height = info_header->height;
    int top_down = 0;

    if (width < 0) width = -width;
    if (height < 0) {
        height = -height;
        top_down = 1;
    }

    // Allocate image structure
    bmp_image_t *image = (bmp_image_t *)kmalloc(sizeof(bmp_image_t));
    if (!image) return 0;

    image->width = width;
    image->height = height;
    image->pixels = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));

    if (!image->pixels) {
        kfree(image);
        return 0;
    }

    // Get pixel data
    const uint8_t *pixel_data = data + file_header->data_offset;
    uint16_t bpp = info_header->bits_per_pixel;

    // Calculate row padding (rows are aligned to 4 bytes)
    uint32_t row_size = ((bpp * width + 31) / 32) * 4;

    // Read pixels
    for (int32_t y = 0; y < height; y++) {
        // BMP stores rows bottom-to-top unless negative height
        int32_t src_y = top_down ? y : (height - 1 - y);
        const uint8_t *row = pixel_data + src_y * row_size;

        for (int32_t x = 0; x < width; x++) {
            uint32_t pixel;

            if (bpp == 24) {
                // BGR format
                uint8_t b = row[x * 3];
                uint8_t g = row[x * 3 + 1];
                uint8_t r = row[x * 3 + 2];
                pixel = (0xFF << 24) | (r << 16) | (g << 8) | b;
            } else {  // 32 bpp
                // BGRA format
                uint8_t b = row[x * 4];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                uint8_t a = row[x * 4 + 3];
                pixel = (a << 24) | (r << 16) | (g << 8) | b;
            }

            image->pixels[y * width + x] = pixel;
        }
    }

    return image;
}

// Load BMP from file
bmp_image_t *bmp_load_file(const char *path) {
    extern void print_str(char *);
    extern void print_int(int);

    print_str("  bmp_load_file: opening ");
    print_str((char*)path);
    print_str("\n");

    vfs_node_t *node = vfs_open(path, VFS_READ);
    if (!node) {
        print_str("  bmp_load_file: vfs_open failed\n");
        return 0;
    }

    print_str("  bmp_load_file: file size = ");
    print_int(node->length);
    print_str("\n");

    uint8_t *buffer = (uint8_t *)kmalloc(node->length);
    if (!buffer) {
        print_str("  bmp_load_file: kmalloc failed\n");
        vfs_close(node);
        return 0;
    }

    print_str("  bmp_load_file: reading file...\n");
    int bytes_read = vfs_read(node, 0, node->length, buffer);
    vfs_close(node);

    print_str("  bmp_load_file: read ");
    print_int(bytes_read);
    print_str(" bytes\n");

    if (bytes_read <= 0) {
        kfree(buffer);
        return 0;
    }

    print_str("  bmp_load_file: parsing BMP...\n");
    bmp_image_t *image = bmp_load(buffer, bytes_read);
    kfree(buffer);

    if (image) {
        print_str("  bmp_load_file: success! ");
        print_int(image->width);
        print_str("x");
        print_int(image->height);
        print_str("\n");
    } else {
        print_str("  bmp_load_file: bmp_load failed\n");
    }

    return image;
}

// Free BMP image
void bmp_free(bmp_image_t *image) {
    if (image) {
        if (image->pixels) {
            kfree(image->pixels);
        }
        kfree(image);
    }
}
