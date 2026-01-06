#include "gui/png.h"
#include "gui/bmp.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "fs/vfs.h"

// PNG file signature
static const uint8_t PNG_SIGNATURE[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

// PNG chunk types
#define CHUNK_IHDR 0x49484452
#define CHUNK_IDAT 0x49444154
#define CHUNK_IEND 0x49454E44
#define CHUNK_PLTE 0x504C5445

// Color types
#define PNG_GRAYSCALE 0
#define PNG_TRUECOLOR 2
#define PNG_INDEXED 3
#define PNG_GRAYSCALE_ALPHA 4
#define PNG_TRUECOLOR_ALPHA 6

// Filter types
#define FILTER_NONE 0
#define FILTER_SUB 1
#define FILTER_UP 2
#define FILTER_AVERAGE 3
#define FILTER_PAETH 4

// Read big-endian 32-bit
static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

// DEFLATE decompression state
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
    uint32_t bit_buffer;
    int bits_in_buffer;
} inflate_state_t;

// Get a single bit
static int get_bit(inflate_state_t *s) {
    if (s->bits_in_buffer == 0) {
        if (s->pos >= s->size) return -1;
        s->bit_buffer = s->data[s->pos++];
        s->bits_in_buffer = 8;
    }
    int bit = s->bit_buffer & 1;
    s->bit_buffer >>= 1;
    s->bits_in_buffer--;
    return bit;
}

// Get n bits (LSB first)
static int get_bits(inflate_state_t *s, int n) {
    int value = 0;
    for (int i = 0; i < n; i++) {
        int bit = get_bit(s);
        if (bit < 0) return -1;
        value |= (bit << i);
    }
    return value;
}

// Align to byte boundary
static void align_byte(inflate_state_t *s) {
    s->bits_in_buffer = 0;
    s->bit_buffer = 0;
}


// Length base values
static const int length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
    15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
    67, 83, 99, 115, 131, 163, 195, 227, 258
};

// Length extra bits
static const int length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5, 0
};

// Distance base values
static const int dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
    33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

// Distance extra bits
static const int dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3,
    4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

// Decode fixed Huffman literal/length
static int decode_fixed_lit(inflate_state_t *s) {
    // Read 7 bits first
    int code = 0;
    for (int i = 0; i < 7; i++) {
        int bit = get_bit(s);
        if (bit < 0) return -1;
        code = (code << 1) | bit;
    }

    // 256-279: 7-bit codes (0000000-0010111 reversed = 256-279)
    if (code >= 0 && code <= 23) {
        return code + 256;
    }

    // Need 8th bit
    int bit = get_bit(s);
    if (bit < 0) return -1;
    code = (code << 1) | bit;

    // 0-143: 8-bit codes (00110000-10111111 reversed)
    if (code >= 48 && code <= 191) {
        return code - 48;
    }
    // 280-287: 8-bit codes (11000000-11000111 reversed)
    if (code >= 192 && code <= 199) {
        return code - 192 + 280;
    }

    // Need 9th bit for 144-255
    bit = get_bit(s);
    if (bit < 0) return -1;
    code = (code << 1) | bit;

    // 144-255: 9-bit codes (110010000-111111111 reversed)
    if (code >= 400 && code <= 511) {
        return code - 400 + 144;
    }

    return -1;
}

// Decode fixed Huffman distance (5 bits, no actual huffman)
static int decode_fixed_dist(inflate_state_t *s) {
    int code = 0;
    for (int i = 0; i < 5; i++) {
        int bit = get_bit(s);
        if (bit < 0) return -1;
        code = (code << 1) | bit;
    }
    return code;
}

// Simple DEFLATE inflate for fixed and uncompressed blocks
static int inflate_data(const uint8_t *src, size_t src_size,
                        uint8_t *dst, size_t dst_size, size_t *out_size) {
    inflate_state_t state = {
        .data = src,
        .size = src_size,
        .pos = 0,
        .bit_buffer = 0,
        .bits_in_buffer = 0
    };

    // Skip zlib header (2 bytes)
    if (src_size < 2) return -1;
    state.pos = 2;

    size_t out_pos = 0;
    int bfinal = 0;

    while (!bfinal && out_pos < dst_size) {
        bfinal = get_bit(&state);
        int btype = get_bits(&state, 2);

        if (btype == 0) {
            // Uncompressed block
            align_byte(&state);
            if (state.pos + 4 > src_size) return -1;

            uint16_t len = state.data[state.pos] | (state.data[state.pos + 1] << 8);
            state.pos += 4;  // Skip len and nlen

            if (state.pos + len > src_size) return -1;
            if (out_pos + len > dst_size) return -1;

            memcpy(dst + out_pos, state.data + state.pos, len);
            out_pos += len;
            state.pos += len;

        } else if (btype == 1) {
            // Fixed Huffman codes
            while (out_pos < dst_size) {
                int sym = decode_fixed_lit(&state);
                if (sym < 0) return -1;

                if (sym < 256) {
                    // Literal byte
                    dst[out_pos++] = (uint8_t)sym;
                } else if (sym == 256) {
                    // End of block
                    break;
                } else {
                    // Length/distance pair
                    int len_idx = sym - 257;
                    if (len_idx >= 29) return -1;

                    int length = length_base[len_idx];
                    if (length_extra[len_idx] > 0) {
                        length += get_bits(&state, length_extra[len_idx]);
                    }

                    int dist_code = decode_fixed_dist(&state);
                    if (dist_code < 0 || dist_code >= 30) return -1;

                    int distance = dist_base[dist_code];
                    if (dist_extra[dist_code] > 0) {
                        distance += get_bits(&state, dist_extra[dist_code]);
                    }

                    // Copy from back buffer
                    if ((size_t)distance > out_pos) return -1;
                    for (int i = 0; i < length && out_pos < dst_size; i++) {
                        dst[out_pos] = dst[out_pos - distance];
                        out_pos++;
                    }
                }
            }

        } else if (btype == 2) {
            // Dynamic Huffman - simplified: just fail for now
            // Most PNG files use fixed or uncompressed
            return -1;

        } else {
            return -1;  // Invalid block type
        }
    }

    *out_size = out_pos;
    return 0;
}

// Paeth predictor
static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;

    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// Apply PNG filters
static int apply_filters(uint8_t *data, uint32_t width, uint32_t height,
                         int bytes_per_pixel) {
    uint32_t row_bytes = width * bytes_per_pixel;
    uint32_t filtered_row_bytes = row_bytes + 1;  // +1 for filter byte

    uint8_t *prev_row = 0;

    for (uint32_t y = 0; y < height; y++) {
        uint8_t *row = data + y * filtered_row_bytes;
        uint8_t filter = row[0];
        uint8_t *pixels = row + 1;

        switch (filter) {
            case FILTER_NONE:
                break;

            case FILTER_SUB:
                for (uint32_t x = bytes_per_pixel; x < row_bytes; x++) {
                    pixels[x] += pixels[x - bytes_per_pixel];
                }
                break;

            case FILTER_UP:
                if (prev_row) {
                    for (uint32_t x = 0; x < row_bytes; x++) {
                        pixels[x] += prev_row[x + 1];
                    }
                }
                break;

            case FILTER_AVERAGE:
                for (uint32_t x = 0; x < row_bytes; x++) {
                    uint8_t left = (x >= (uint32_t)bytes_per_pixel) ?
                                   pixels[x - bytes_per_pixel] : 0;
                    uint8_t up = prev_row ? prev_row[x + 1] : 0;
                    pixels[x] += (left + up) / 2;
                }
                break;

            case FILTER_PAETH:
                for (uint32_t x = 0; x < row_bytes; x++) {
                    uint8_t left = (x >= (uint32_t)bytes_per_pixel) ?
                                   pixels[x - bytes_per_pixel] : 0;
                    uint8_t up = prev_row ? prev_row[x + 1] : 0;
                    uint8_t up_left = (prev_row && x >= (uint32_t)bytes_per_pixel) ?
                                      prev_row[x + 1 - bytes_per_pixel] : 0;
                    pixels[x] += paeth(left, up, up_left);
                }
                break;

            default:
                return -1;  // Unknown filter
        }

        prev_row = row;
    }

    return 0;
}

// Load PNG from memory
png_image_t *png_load(const uint8_t *data, size_t size) {
    // Check signature
    if (size < 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (data[i] != PNG_SIGNATURE[i]) return 0;
    }

    // Parse chunks
    size_t pos = 8;
    uint32_t width = 0, height = 0;
    uint8_t bit_depth = 0, color_type = 0;

    // Collect all IDAT data
    uint8_t *idat_data = 0;
    size_t idat_size = 0;
    size_t idat_capacity = 0;

    while (pos + 12 <= size) {
        uint32_t chunk_len = read_be32(data + pos);
        uint32_t chunk_type = read_be32(data + pos + 4);
        const uint8_t *chunk_data = data + pos + 8;

        if (pos + 12 + chunk_len > size) break;

        if (chunk_type == CHUNK_IHDR) {
            if (chunk_len < 13) goto error;
            width = read_be32(chunk_data);
            height = read_be32(chunk_data + 4);
            bit_depth = chunk_data[8];
            color_type = chunk_data[9];

            // Only support 8-bit truecolor (RGB) and truecolor+alpha (RGBA)
            if (bit_depth != 8) goto error;
            if (color_type != PNG_TRUECOLOR && color_type != PNG_TRUECOLOR_ALPHA) {
                goto error;
            }

        } else if (chunk_type == CHUNK_IDAT) {
            // Append to IDAT buffer
            if (idat_size + chunk_len > idat_capacity) {
                size_t new_cap = (idat_capacity == 0) ? 65536 : idat_capacity * 2;
                while (new_cap < idat_size + chunk_len) new_cap *= 2;
                uint8_t *new_buf = (uint8_t *)kmalloc(new_cap);
                if (!new_buf) goto error;
                if (idat_data) {
                    memcpy(new_buf, idat_data, idat_size);
                    kfree(idat_data);
                }
                idat_data = new_buf;
                idat_capacity = new_cap;
            }
            memcpy(idat_data + idat_size, chunk_data, chunk_len);
            idat_size += chunk_len;

        } else if (chunk_type == CHUNK_IEND) {
            break;
        }

        pos += 12 + chunk_len;
    }

    if (!width || !height || !idat_data) goto error;

    // Decompress IDAT
    int bytes_per_pixel = (color_type == PNG_TRUECOLOR_ALPHA) ? 4 : 3;
    size_t raw_size = (width * bytes_per_pixel + 1) * height;  // +1 per row for filter
    uint8_t *raw_data = (uint8_t *)kmalloc(raw_size);
    if (!raw_data) goto error;

    size_t decompressed_size = 0;
    if (inflate_data(idat_data, idat_size, raw_data, raw_size, &decompressed_size) != 0) {
        kfree(raw_data);
        goto error;
    }

    kfree(idat_data);
    idat_data = 0;

    // Apply PNG filters
    if (apply_filters(raw_data, width, height, bytes_per_pixel) != 0) {
        kfree(raw_data);
        return 0;
    }

    // Allocate image
    png_image_t *img = (png_image_t *)kmalloc(sizeof(png_image_t));
    if (!img) {
        kfree(raw_data);
        return 0;
    }

    img->width = width;
    img->height = height;
    img->pixels = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    if (!img->pixels) {
        kfree(raw_data);
        kfree(img);
        return 0;
    }

    // Convert to ARGB
    uint32_t row_bytes = width * bytes_per_pixel + 1;
    for (uint32_t y = 0; y < height; y++) {
        uint8_t *row = raw_data + y * row_bytes + 1;  // Skip filter byte
        for (uint32_t x = 0; x < width; x++) {
            uint8_t r = row[x * bytes_per_pixel];
            uint8_t g = row[x * bytes_per_pixel + 1];
            uint8_t b = row[x * bytes_per_pixel + 2];
            uint8_t a = (bytes_per_pixel == 4) ? row[x * bytes_per_pixel + 3] : 0xFF;
            img->pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    kfree(raw_data);
    return img;

error:
    if (idat_data) kfree(idat_data);
    return 0;
}

// Load PNG from file
png_image_t *png_load_file(const char *path) {
    vfs_node_t *node = vfs_open(path, VFS_READ);
    if (!node) return 0;

    uint8_t *buffer = (uint8_t *)kmalloc(node->length);
    if (!buffer) {
        vfs_close(node);
        return 0;
    }

    int bytes_read = vfs_read(node, 0, node->length, buffer);
    vfs_close(node);

    if (bytes_read <= 0) {
        kfree(buffer);
        return 0;
    }

    png_image_t *img = png_load(buffer, bytes_read);
    kfree(buffer);
    return img;
}

// Free PNG image
void png_free(png_image_t *img) {
    if (img) {
        if (img->pixels) kfree(img->pixels);
        kfree(img);
    }
}
