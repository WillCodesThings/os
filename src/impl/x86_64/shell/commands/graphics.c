// Graphics commands: draw, cls, view

#include <shell/commands.h>
#include <shell/print.h>
#include <graphics/graphics.h>
#include <gui/imageviewer.h>

// Helper to parse color string
static int cmd_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

uint32_t parse_color(const char *str)
{
    if (cmd_strcmp(str, "red") == 0)
        return COLOR_RED;
    if (cmd_strcmp(str, "green") == 0)
        return COLOR_GREEN;
    if (cmd_strcmp(str, "blue") == 0)
        return COLOR_BLUE;
    if (cmd_strcmp(str, "white") == 0)
        return COLOR_WHITE;
    if (cmd_strcmp(str, "yellow") == 0)
        return COLOR_YELLOW;
    if (cmd_strcmp(str, "cyan") == 0)
        return COLOR_CYAN;
    if (cmd_strcmp(str, "magenta") == 0)
        return COLOR_MAGENTA;
    if (cmd_strcmp(str, "black") == 0)
        return COLOR_BLACK;
    return COLOR_WHITE;
}

void cmd_cls(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    clear_screen(COLOR_BLACK);
    print_str("Graphics screen cleared\n");
}

void cmd_draw(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: draw <shape> [args]\n");
        print_str("Shapes:\n");
        print_str("  triangle <x0> <y0> <x1> <y1> <x2> <y2> <color> <filled>\n");
        print_str("  rect <x> <y> <w> <h> <color>\n");
        print_str("  line <x0> <y0> <x1> <y1> <color>\n");
        print_str("  pixel <x> <y> <color>\n");
        print_str("  circle <cx> <cy> <radius> <color> [filled]\n");
        print_str("Colors: red, green, blue, white, yellow, cyan, magenta\n");
        return;
    }

    if (cmd_strcmp(argv[1], "triangle") == 0)
    {
        if (argc < 10)
        {
            print_str("Usage: draw triangle <x0> <y0> <x1> <y1> <x2> <y2> <color> <filled>\n");
            return;
        }

        int x0 = atoi(argv[2]);
        int y0 = atoi(argv[3]);
        int x1 = atoi(argv[4]);
        int y1 = atoi(argv[5]);
        int x2 = atoi(argv[6]);
        int y2 = atoi(argv[7]);
        uint32_t color = parse_color(argv[8]);
        int isFilled = (argc >= 10 && cmd_strcmp(argv[9], "filled") == 0);

        draw_triangle(x0, y0, x1, y1, x2, y2, color, isFilled);
        print_str("Triangle drawn!\n");
    }
    else if (cmd_strcmp(argv[1], "rect") == 0)
    {
        if (argc < 7)
        {
            print_str("Usage: draw rect <x> <y> <w> <h> <color>\n");
            return;
        }

        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        int w = atoi(argv[4]);
        int h = atoi(argv[5]);
        uint32_t color = parse_color(argv[6]);

        fill_rect(x, y, w, h, color);
        print_str("Rectangle drawn!\n");
    }
    else if (cmd_strcmp(argv[1], "line") == 0)
    {
        if (argc < 7)
        {
            print_str("Usage: draw line <x0> <y0> <x1> <y1> <color>\n");
            return;
        }

        int x0 = atoi(argv[2]);
        int y0 = atoi(argv[3]);
        int x1 = atoi(argv[4]);
        int y1 = atoi(argv[5]);
        uint32_t color = parse_color(argv[6]);

        draw_line(x0, y0, x1, y1, color);
        print_str("Line drawn!\n");
    }
    else if (cmd_strcmp(argv[1], "pixel") == 0)
    {
        if (argc < 5)
        {
            print_str("Usage: draw pixel <x> <y> <color>\n");
            return;
        }

        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        uint32_t color = parse_color(argv[4]);

        put_pixel(x, y, color);
        print_str("Pixel drawn!\n");
    }
    else if (cmd_strcmp(argv[1], "circle") == 0)
    {
        if (argc < 6)
        {
            print_str("Usage: draw circle <cx> <cy> <radius> <color> [filled]\n");
            return;
        }

        int cx = atoi(argv[2]);
        int cy = atoi(argv[3]);
        int radius = atoi(argv[4]);
        uint32_t color = parse_color(argv[5]);
        int isFilled = (argc >= 7 && cmd_strcmp(argv[6], "filled") == 0);

        draw_circle(cx, cy, radius, color, isFilled);
        print_str("Circle drawn!\n");
    }
    else
    {
        print_str("Unknown shape: ");
        print_str(argv[1]);
        print_str("\n");
    }
}

void cmd_view(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: view <image.bmp>\n");
        return;
    }

    print_str("Opening image: ");
    print_str(argv[1]);
    print_str("\n");

    print_str("Creating viewer window...\n");
    image_viewer_t *viewer = imageviewer_create("Image Viewer", 100, 100);
    if (!viewer)
    {
        print_str("Failed to create viewer window\n");
        return;
    }
    print_str("Window created.\n");

    print_str("Loading image file...\n");
    if (imageviewer_load_file(viewer, argv[1]) != 0)
    {
        print_str("Failed to load image: ");
        print_str(argv[1]);
        print_str("\n");
        imageviewer_close(viewer);
        return;
    }

    print_str("Image loaded successfully!\n");
    print_str("Drag the title bar to move. Click X to close.\n");
}
