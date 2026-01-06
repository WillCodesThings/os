#include "gui/jpg.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "fs/vfs.h"

// JPEG markers
#define MARKER_SOI  0xD8  // Start of Image
#define MARKER_EOI  0xD9  // End of Image
#define MARKER_SOF0 0xC0  // Baseline DCT
#define MARKER_SOF2 0xC2  // Progressive DCT (not supported)
#define MARKER_DHT  0xC4  // Define Huffman Table
#define MARKER_DQT  0xDB  // Define Quantization Table
#define MARKER_DRI  0xDD  // Define Restart Interval
#define MARKER_SOS  0xDA  // Start of Scan
#define MARKER_APP0 0xE0  // JFIF
#define MARKER_COM  0xFE  // Comment

// Maximum values
#define MAX_COMPONENTS 3
#define MAX_HUFFMAN_TABLES 4

// Zigzag order
static const uint8_t zigzag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10,
    17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

// Huffman table
typedef struct {
    uint8_t bits[16];      // Number of codes of each length
    uint8_t values[256];   // Symbol values
    uint16_t codes[256];   // Huffman codes
    int sizes[256];        // Code lengths
    int num_codes;
} huffman_table_t;

// JPEG decoder state
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;

    uint32_t width;
    uint32_t height;
    uint8_t num_components;

    // Component info
    struct {
        uint8_t id;
        uint8_t h_sample;  // Horizontal sampling factor
        uint8_t v_sample;  // Vertical sampling factor
        uint8_t qt_id;     // Quantization table ID
        uint8_t dc_table;  // DC Huffman table
        uint8_t ac_table;  // AC Huffman table
    } components[MAX_COMPONENTS];

    // Quantization tables (up to 4)
    int16_t quant[4][64];

    // Huffman tables (DC and AC, up to 2 each)
    huffman_table_t huff_dc[2];
    huffman_table_t huff_ac[2];

    // Bit reading state
    uint32_t bit_buffer;
    int bits_left;

    // DC prediction values
    int16_t dc_pred[MAX_COMPONENTS];

    // Restart interval
    uint16_t restart_interval;
    uint16_t restart_count;
} jpeg_decoder_t;

// Read 16-bit big-endian
static uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

// Get next byte (handles FF00 stuffing)
static int get_byte(jpeg_decoder_t *d) {
    if (d->pos >= d->size) return -1;
    uint8_t b = d->data[d->pos++];

    // Handle byte stuffing (FF00 -> FF)
    if (b == 0xFF && d->pos < d->size && d->data[d->pos] == 0x00) {
        d->pos++;
    }

    return b;
}

// Get n bits from bitstream
static int get_bits(jpeg_decoder_t *d, int n) {
    while (d->bits_left < n) {
        int b = get_byte(d);
        if (b < 0) return -1;
        d->bit_buffer = (d->bit_buffer << 8) | b;
        d->bits_left += 8;
    }

    d->bits_left -= n;
    return (d->bit_buffer >> d->bits_left) & ((1 << n) - 1);
}

// Decode Huffman symbol
static int decode_huffman(jpeg_decoder_t *d, huffman_table_t *table) {
    int code = 0;

    for (int len = 1; len <= 16; len++) {
        int bit = get_bits(d, 1);
        if (bit < 0) return -1;
        code = (code << 1) | bit;

        // Search for matching code
        int idx = 0;
        for (int i = 0; i < len - 1; i++) {
            idx += table->bits[i];
        }
        for (int i = 0; i < table->bits[len - 1]; i++) {
            if (table->sizes[idx + i] == len && table->codes[idx + i] == code) {
                return table->values[idx + i];
            }
        }
    }

    return -1;
}

// Extend sign bit
static int extend(int value, int bits) {
    if (bits == 0) return 0;
    int threshold = 1 << (bits - 1);
    if (value < threshold) {
        value -= (1 << bits) - 1;
    }
    return value;
}

// Build Huffman table codes
static void build_huffman_codes(huffman_table_t *table) {
    int code = 0;
    int idx = 0;

    for (int len = 1; len <= 16; len++) {
        for (int i = 0; i < table->bits[len - 1]; i++) {
            table->codes[idx] = code;
            table->sizes[idx] = len;
            idx++;
            code++;
        }
        code <<= 1;
    }
    table->num_codes = idx;
}

// Simple integer IDCT (scaled, approximate)
static void idct_block(int16_t *block) {
    // Very simplified IDCT using integer math
    // This is a scaled approximation, not exact
    int tmp[64];

    // Row IDCT
    for (int i = 0; i < 8; i++) {
        int *row = &tmp[i * 8];
        int16_t *src = &block[i * 8];

        // DC term (scaled by 8)
        int dc = src[0] * 8;

        // Simplified: just use DC for now (very basic approximation)
        // A proper IDCT would compute all 64 terms
        int s1 = src[1] * 7;
        int s2 = src[2] * 6;
        int s3 = src[3] * 4;
        int s4 = src[4] * 3;

        row[0] = dc + s1 + s2 + s3 + s4;
        row[1] = dc + s1 + s2 + s3 - s4;
        row[2] = dc + s1 + s2 - s3;
        row[3] = dc + s1 - s2;
        row[4] = dc - s1;
        row[5] = dc - s1 - s2;
        row[6] = dc - s1 - s2 - s3;
        row[7] = dc - s1 - s2 - s3 - s4;
    }

    // Column IDCT + final scaling
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int val = tmp[j * 8 + i];
            // Scale down and add 128 (level shift)
            val = (val / 64) + 128;
            // Clamp
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            block[j * 8 + i] = val;
        }
    }
}

// Decode one 8x8 block
static int decode_block(jpeg_decoder_t *d, int16_t *block, int comp) {
    memset(block, 0, 64 * sizeof(int16_t));

    // DC coefficient
    int dc_table = d->components[comp].dc_table;
    int symbol = decode_huffman(d, &d->huff_dc[dc_table]);
    if (symbol < 0) return -1;

    if (symbol > 0) {
        int value = get_bits(d, symbol);
        if (value < 0) return -1;
        value = extend(value, symbol);
        d->dc_pred[comp] += value;
    }
    block[0] = d->dc_pred[comp] * d->quant[d->components[comp].qt_id][0];

    // AC coefficients
    int ac_table = d->components[comp].ac_table;
    int idx = 1;

    while (idx < 64) {
        symbol = decode_huffman(d, &d->huff_ac[ac_table]);
        if (symbol < 0) return -1;

        if (symbol == 0) {
            // End of block
            break;
        }

        int zeros = (symbol >> 4) & 0x0F;
        int bits = symbol & 0x0F;

        if (bits == 0) {
            if (zeros == 15) {
                // 16 zeros
                idx += 16;
            } else {
                break;
            }
        } else {
            idx += zeros;
            if (idx >= 64) break;

            int value = get_bits(d, bits);
            if (value < 0) return -1;
            value = extend(value, bits);

            int zig_idx = zigzag[idx];
            block[zig_idx] = value * d->quant[d->components[comp].qt_id][zig_idx];
            idx++;
        }
    }

    // Apply IDCT
    idct_block(block);

    return 0;
}

// Parse JPEG headers
static int parse_headers(jpeg_decoder_t *d) {
    // Check SOI marker
    if (d->pos + 2 > d->size) return -1;
    if (d->data[0] != 0xFF || d->data[1] != MARKER_SOI) return -1;
    d->pos = 2;

    while (d->pos + 2 <= d->size) {
        if (d->data[d->pos] != 0xFF) {
            d->pos++;
            continue;
        }

        uint8_t marker = d->data[d->pos + 1];
        d->pos += 2;

        // Skip padding FF bytes
        while (marker == 0xFF && d->pos < d->size) {
            marker = d->data[d->pos++];
        }

        if (marker == MARKER_EOI) break;
        if (marker == MARKER_SOS) break;  // Start of scan - stop parsing headers

        // Get segment length
        if (d->pos + 2 > d->size) return -1;
        uint16_t length = read_be16(d->data + d->pos);
        d->pos += 2;

        if (d->pos + length - 2 > d->size) return -1;
        const uint8_t *segment = d->data + d->pos;

        switch (marker) {
            case MARKER_SOF0: {
                // Baseline DCT
                if (length < 11) return -1;
                d->height = read_be16(segment + 1);
                d->width = read_be16(segment + 3);
                d->num_components = segment[5];

                if (d->num_components > MAX_COMPONENTS) return -1;

                for (int i = 0; i < d->num_components; i++) {
                    d->components[i].id = segment[6 + i * 3];
                    uint8_t sampling = segment[7 + i * 3];
                    d->components[i].h_sample = (sampling >> 4) & 0x0F;
                    d->components[i].v_sample = sampling & 0x0F;
                    d->components[i].qt_id = segment[8 + i * 3];
                }
                break;
            }

            case MARKER_DHT: {
                // Define Huffman Table
                size_t offset = 0;
                while (offset + 17 <= length - 2) {
                    uint8_t info = segment[offset++];
                    int table_class = (info >> 4) & 1;  // 0 = DC, 1 = AC
                    int table_id = info & 0x0F;

                    if (table_id > 1) return -1;

                    huffman_table_t *table = table_class ?
                                             &d->huff_ac[table_id] : &d->huff_dc[table_id];

                    int num_codes = 0;
                    for (int i = 0; i < 16; i++) {
                        table->bits[i] = segment[offset++];
                        num_codes += table->bits[i];
                    }

                    if (offset + num_codes > length - 2) return -1;
                    for (int i = 0; i < num_codes; i++) {
                        table->values[i] = segment[offset++];
                    }

                    build_huffman_codes(table);
                }
                break;
            }

            case MARKER_DQT: {
                // Define Quantization Table
                size_t offset = 0;
                while (offset + 65 <= length - 2) {
                    uint8_t info = segment[offset++];
                    int precision = (info >> 4) & 0x0F;
                    int table_id = info & 0x0F;

                    if (table_id > 3) return -1;
                    if (precision != 0) return -1;  // Only 8-bit precision

                    for (int i = 0; i < 64; i++) {
                        d->quant[table_id][i] = segment[offset++];
                    }
                }
                break;
            }

            case MARKER_DRI: {
                // Define Restart Interval
                if (length >= 4) {
                    d->restart_interval = read_be16(segment);
                }
                break;
            }
        }

        d->pos += length - 2;
    }

    return 0;
}

// Parse SOS (Start of Scan) and decode
static int decode_scan(jpeg_decoder_t *d) {
    // Find SOS marker
    while (d->pos + 2 <= d->size) {
        if (d->data[d->pos] == 0xFF && d->data[d->pos + 1] == MARKER_SOS) {
            d->pos += 2;
            break;
        }
        d->pos++;
    }

    if (d->pos + 2 > d->size) return -1;

    // Parse SOS header
    d->pos += 2;  // Skip length field

    uint8_t num_scan_comp = d->data[d->pos++];
    if (num_scan_comp != d->num_components) return -1;

    for (int i = 0; i < num_scan_comp; i++) {
        uint8_t comp_id = d->data[d->pos++];
        uint8_t table_sel = d->data[d->pos++];

        // Find component and set table selections
        for (int j = 0; j < d->num_components; j++) {
            if (d->components[j].id == comp_id) {
                d->components[j].dc_table = (table_sel >> 4) & 0x0F;
                d->components[j].ac_table = table_sel & 0x0F;
                break;
            }
        }
    }

    d->pos += 3;  // Skip Ss, Se, Ah/Al

    // Reset bit reading state
    d->bit_buffer = 0;
    d->bits_left = 0;

    // Reset DC prediction
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        d->dc_pred[i] = 0;
    }

    return 0;
}

// Load JPEG from memory
jpg_image_t *jpg_load(const uint8_t *data, size_t size) {
    if (!data || size < 4) return 0;

    jpeg_decoder_t decoder;
    memset(&decoder, 0, sizeof(decoder));
    decoder.data = data;
    decoder.size = size;

    // Parse headers
    if (parse_headers(&decoder) != 0) return 0;
    if (decoder.width == 0 || decoder.height == 0) return 0;

    // Only support YCbCr (3 components) for now
    if (decoder.num_components != 3) return 0;

    // Parse SOS and prepare for decoding
    if (decode_scan(&decoder) != 0) return 0;

    // Allocate output image
    jpg_image_t *img = (jpg_image_t *)kmalloc(sizeof(jpg_image_t));
    if (!img) return 0;

    img->width = decoder.width;
    img->height = decoder.height;
    img->pixels = (uint32_t *)kmalloc(img->width * img->height * sizeof(uint32_t));
    if (!img->pixels) {
        kfree(img);
        return 0;
    }

    // Decode MCUs (Minimum Coded Units)
    // For simplicity, assume 1x1 sampling (no subsampling)
    int mcu_width = 8;
    int mcu_height = 8;
    int mcus_x = (img->width + mcu_width - 1) / mcu_width;
    int mcus_y = (img->height + mcu_height - 1) / mcu_height;

    int16_t blocks[3][64];  // Y, Cb, Cr blocks

    decoder.restart_count = decoder.restart_interval;

    for (int mcu_y = 0; mcu_y < mcus_y; mcu_y++) {
        for (int mcu_x = 0; mcu_x < mcus_x; mcu_x++) {
            // Check for restart marker
            if (decoder.restart_interval > 0 && decoder.restart_count == 0) {
                // Find restart marker and reset
                decoder.bits_left = 0;
                while (decoder.pos + 1 < decoder.size) {
                    if (decoder.data[decoder.pos] == 0xFF) {
                        uint8_t m = decoder.data[decoder.pos + 1];
                        if (m >= 0xD0 && m <= 0xD7) {
                            decoder.pos += 2;
                            break;
                        }
                    }
                    decoder.pos++;
                }
                for (int i = 0; i < MAX_COMPONENTS; i++) {
                    decoder.dc_pred[i] = 0;
                }
                decoder.restart_count = decoder.restart_interval;
            }

            // Decode blocks for each component
            for (int comp = 0; comp < decoder.num_components; comp++) {
                if (decode_block(&decoder, blocks[comp], comp) != 0) {
                    kfree(img->pixels);
                    kfree(img);
                    return 0;
                }
            }

            if (decoder.restart_interval > 0) {
                decoder.restart_count--;
            }

            // Convert YCbCr to RGB and write to image
            for (int by = 0; by < 8; by++) {
                int py = mcu_y * 8 + by;
                if (py >= (int)img->height) continue;

                for (int bx = 0; bx < 8; bx++) {
                    int px = mcu_x * 8 + bx;
                    if (px >= (int)img->width) continue;

                    int idx = by * 8 + bx;
                    int y = blocks[0][idx];
                    int cb = blocks[1][idx] - 128;
                    int cr = blocks[2][idx] - 128;

                    // YCbCr to RGB conversion (integer approximation)
                    int r = y + ((cr * 359) >> 8);
                    int g = y - ((cb * 88 + cr * 183) >> 8);
                    int b = y + ((cb * 454) >> 8);

                    // Clamp
                    if (r < 0) r = 0;
                    if (r > 255) r = 255;
                    if (g < 0) g = 0;
                    if (g > 255) g = 255;
                    if (b < 0) b = 0;
                    if (b > 255) b = 255;

                    img->pixels[py * img->width + px] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }
    }

    return img;
}

// Load JPEG from file
jpg_image_t *jpg_load_file(const char *path) {
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

    jpg_image_t *img = jpg_load(buffer, bytes_read);
    kfree(buffer);
    return img;
}

// Free JPEG image
void jpg_free(jpg_image_t *img) {
    if (img) {
        if (img->pixels) kfree(img->pixels);
        kfree(img);
    }
}
