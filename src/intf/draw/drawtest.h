#ifndef DRAWTEST_H
#define DRAWTEST_H

#include <stdint.h>

#define VBE_MODE 0x118 // 1024x768x32bpp

// VBE Mode Info structure (partial, packed to match VBE spec)
typedef struct
{
    uint16_t attributes;
    uint8_t winA, winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segmentA;
    uint16_t segmentB;
    uint32_t realFctPtr;
    uint16_t pitch;
    uint16_t XResolution;
    uint16_t YResolution;
    uint8_t XCharSize;
    uint8_t YCharSize;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    uint32_t framebuffer; // physical address of framebuffer
} __attribute__((packed)) VbeModeInfo;

extern volatile VbeModeInfo *vbe_mode_info;
extern volatile uint32_t framebuffer_address;
extern volatile uint16_t screen_width;
extern volatile uint16_t screen_height;
extern volatile uint8_t bits_per_pixel;
extern volatile uint16_t pitch;

// Get VBE mode info (usually done in assembly)
void get_mode_info(void);

// Draw a single pixel (32-bit color) at (x,y)
void putpixel(uint32_t *framebuffer, int x, int y, int pitch, uint32_t color);

// Draw a filled rectangle with RGB color
void fillrect(uint32_t *framebuffer, int pitch, int x, int y, int w, int h,
              uint8_t r, uint8_t g, uint8_t b);

#endif // DRAWTEST_H
