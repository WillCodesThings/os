// #include <stdint.h>
// #include <draw/drawtest.h>

// uint32_t *framebuffer = NULL;
// uint32_t screen_width = 0;
// uint32_t screen_height = 0;
// uint32_t pitch = 0;

// void init_graphics()
// {
//     screen_width = mib->XResolution;
//     screen_height = mib->YResolution;
//     pitch = mib->BytesPerScanLine;

//     printf("Screen Resolution: %dx%d\n bpp\n", screen_width, screen_height, mib->BitsPerPixel);
//     printf("Pitch: %d bytes\n", pitch);
// }

// void put_pixel(uint32_t x, uint32_t y, uint32_t color)
// {
//     if (x >= screen_width || y >= screen_height)
//         return; // Out of bounds

//     uint32_t *pixel_address = (uint32_t *)((uint8_t *)framebuffer + y * pitch + x * 4);
//     *pixel_address = color;
// }