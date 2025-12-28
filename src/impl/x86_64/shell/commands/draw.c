// #include <shell/commands/draw.h>
// #include <graphics/graphics.h>
// #include <shell/shell.h>

// uint32_t parse_color(const char *str)
// {
//     if (strcmp(str, "red") == 0)
//         return COLOR_RED;
//     if (strcmp(str, "green") == 0)
//         return COLOR_GREEN;
//     if (strcmp(str, "blue") == 0)
//         return COLOR_BLUE;
//     if (strcmp(str, "white") == 0)
//         return COLOR_WHITE;
//     if (strcmp(str, "yellow") == 0)
//         return COLOR_YELLOW;
//     if (strcmp(str, "cyan") == 0)
//         return COLOR_CYAN;
//     if (strcmp(str, "magenta") == 0)
//         return COLOR_MAGENTA;
//     if (strcmp(str, "black") == 0)
//         return COLOR_BLACK;
//     return COLOR_WHITE; // default
// }
// void cmd_draw(int argc, char **argv)
// {
//     if (argc < 2)
//     {
//         print_str("Usage: draw <shape> [args]\n");
//         print_str("Shapes:\n");
//         print_str("  triangle <x0> <y0> <x1> <y1> <x2> <y2> <color> <filled>\n");
//         print_str("  rect <x> <y> <w> <h> <color>\n");
//         print_str("  line <x0> <y0> <x1> <y1> <color>\n");
//         print_str("  pixel <x> <y> <color>\n");
//         print_str("  circle <cx> <cy> <radius> <color> [filled]\n");
//         print_str("Colors: red, green, blue, white, yellow, cyan, magenta\n");
//         return;
//     }

//     if (strcmp(argv[1], "triangle") == 0)
//     {
//         if (argc < 10)
//         {
//             print_str("Usage: draw triangle <x0> <y0> <x1> <y1> <x2> <y2> <color> <filled>\n");
//             return;
//         }

//         int x0 = atoi(argv[2]);
//         int y0 = atoi(argv[3]);
//         int x1 = atoi(argv[4]);
//         int y1 = atoi(argv[5]);
//         int x2 = atoi(argv[6]);
//         int y2 = atoi(argv[7]);
//         uint32_t color = parse_color(argv[8]);
//         int isFilled = 0;

//         if (argc >= 10 && strcmp(argv[9], "filled") == 0)
//         {
//             isFilled = 1;
//         }

//         draw_triangle(x0, y0, x1, y1, x2, y2, color, isFilled);
//         print_str("Triangle drawn!\n");
//     }
//     else if (strcmp(argv[1], "rect") == 0)
//     {
//         if (argc < 7)
//         {
//             print_str("Usage: draw rect <x> <y> <w> <h> <color>\n");
//             return;
//         }

//         int x = atoi(argv[2]);
//         int y = atoi(argv[3]);
//         int w = atoi(argv[4]);
//         int h = atoi(argv[5]);
//         uint32_t color = parse_color(argv[6]);

//         fill_rect(x, y, w, h, color);
//         print_str("Rectangle drawn!\n");
//     }
//     else if (strcmp(argv[1], "line") == 0)
//     {
//         if (argc < 7)
//         {
//             print_str("Usage: draw line <x0> <y0> <x1> <y1> <color>\n");
//             return;
//         }

//         int x0 = atoi(argv[2]);
//         int y0 = atoi(argv[3]);
//         int x1 = atoi(argv[4]);
//         int y1 = atoi(argv[5]);
//         uint32_t color = parse_color(argv[6]);

//         draw_line(x0, y0, x1, y1, color);
//         print_str("Line drawn!\n");
//     }
//     else if (strcmp(argv[1], "pixel") == 0)
//     {
//         if (argc < 5)
//         {
//             print_str("Usage: draw pixel <x> <y> <color>\n");
//             return;
//         }

//         int x = atoi(argv[2]);
//         int y = atoi(argv[3]);
//         uint32_t color = parse_color(argv[4]);

//         put_pixel(x, y, color);
//         print_str("Pixel drawn!\n");
//     }
//     else if (strcmp(argv[1], "circle") == 0)
//     {
//         if (argc < 6)
//         {
//             print_str("Usage: draw circle <cx> <cy> <radius> <color> [filled]\n");
//             return;
//         }

//         int cx = atoi(argv[2]);
//         int cy = atoi(argv[3]);
//         int radius = atoi(argv[4]);
//         uint32_t color = parse_color(argv[5]);
//         int isFilled = 0;

//         if (argc >= 7 && strcmp(argv[6], "filled") == 0)
//         {
//             isFilled = 1;
//         }

//         draw_circle(cx, cy, radius, color, isFilled);
//         print_str("Circle drawn!\n");
//     }
//     else
//     {
//         print_str("Unknown shape: ");
//         print_str(argv[1]);
//         print_str("\n");
//     }
// }