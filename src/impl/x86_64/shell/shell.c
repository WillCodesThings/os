#include <shell/shell.h>

#include <stdint.h>
#include <shell/print.h>
#include <interrupts/io/keyboard.h>
#include <graphics/graphics.h>
#include <interrupts/io/mouse.h>
#include <graphics/cursor.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <interrupts/port_io.h>
#include <memory/heap.h>
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <net/net.h>
#include <net/socket.h>
#include <gui/window.h>
#include <gui/bmp.h>
#include <gui/imageviewer.h>
#include <gui/texteditor.h>
#include <gui/drawapp.h>
#include <gui/guiterminal.h>
#include <gui/desktop.h>
#include <exec/process.h>

// Constants
#define MAX_COMMAND_LENGTH 256
#define MAX_COMMANDS 32
#define MAX_ARGS 16
#define PROMPT "> "

// Command buffer
static char command_buffer[MAX_COMMAND_LENGTH];
static uint16_t buffer_pos = 0;

// Argument parsing
static char *args[MAX_ARGS];
static int arg_count = 0;

// Command function type - now takes argc and argv
typedef void (*command_function)(int argc, char **argv);

// Command structure
typedef struct
{
    const char *name;
    const char *description;
    command_function function;
} command_t;

// Forward declarations for command functions
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_draw(int argc, char **argv);
static void cmd_cls(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_heap(int argc, char **argv);
static void cmd_touch(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_write(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_ping(int argc, char **argv);
static void cmd_ifconfig(int argc, char **argv);
static void cmd_pci(int argc, char **argv);
static void cmd_netstat(int argc, char **argv);
static void cmd_wget(int argc, char **argv);
static void cmd_view(int argc, char **argv);
static void cmd_run(int argc, char **argv);
static void cmd_gui(int argc, char **argv);
static void cmd_edit(int argc, char **argv);

// Command table
static const command_t commands[MAX_COMMANDS] = {
    {"help", "Display available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {"draw", "Draw shapes (draw triangle|rect|line|pixel)", cmd_draw},
    {"cls", "Clear the graphics screen", cmd_cls},
    {"ls", "List directory contents", cmd_ls},
    {"cat", "Display file contents", cmd_cat},
    {"heap", "Show heap statistics", cmd_heap},
    {"touch", "Create a new file", cmd_touch},
    {"rm", "Remove a file", cmd_rm},
    {"write", "Write text to a file", cmd_write},
    {"echo", "Print text to the screen", cmd_echo},
    {"ping", "Ping an IP address (ping <ip>)", cmd_ping},
    {"ifconfig", "Show/set network config (ifconfig [ip] [gateway])", cmd_ifconfig},
    {"pci", "List PCI devices", cmd_pci},
    {"netstat", "Show network status", cmd_netstat},
    {"wget", "Fetch URL content (wget <ip> <port> <path>)", cmd_wget},
    {"view", "Open image viewer (view <file.bmp>)", cmd_view},
    {"run", "Run an ELF binary (run <file>)", cmd_run},
    {"gui", "Launch GUI desktop with apps", cmd_gui},
    {"edit", "Open text editor (edit [file])", cmd_edit},
    {NULL, NULL, NULL} // Sentinel
};

// String comparison function
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

// String length function
static uint8_t strlen(const char *s)
{
    uint8_t len = 0;
    while (s[len])
    {
        len++;
    }
    return len;
}

// Parse command line into arguments
static void parse_arguments(void)
{
    arg_count = 0;
    char *ptr = command_buffer;

    // Skip leading spaces
    while (*ptr == ' ')
        ptr++;

    while (*ptr && arg_count < MAX_ARGS)
    {
        // Start of argument
        args[arg_count++] = ptr;

        // Find end of argument (space or null)
        while (*ptr && *ptr != ' ')
            ptr++;

        // Null-terminate this argument
        if (*ptr)
        {
            *ptr = '\0';
            ptr++;
            // Skip spaces between arguments
            while (*ptr == ' ')
                ptr++;
        }
    }
}

// Execute command
static void execute_command(void)
{
    // Empty command
    if (buffer_pos == 0)
    {
        return;
    }

    // Null-terminate the command
    command_buffer[buffer_pos] = '\0';

    // Parse into arguments
    parse_arguments();

    if (arg_count == 0)
    {
        return;
    }

    // First argument is the command name
    for (int i = 0; i < MAX_COMMANDS; i++)
    {
        if (commands[i].name == NULL)
        {
            break;
        }

        if (strcmp(args[0], commands[i].name) == 0)
        {
            commands[i].function(arg_count, args);
            return;
        }
    }

    // Command not found
    print_str("Unknown command: ");
    print_str(args[0]);
    print_str("\nType 'help' for available commands\n");
}

// Helper function to reset command buffer
static void reset_command_buffer(void)
{
    buffer_pos = 0;
    for (int i = 0; i < MAX_COMMAND_LENGTH; i++)
    {
        command_buffer[i] = 0;
    }
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    if (*str == '-')
    {
        sign = -1;
        str++;
    }

    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

// Helper to parse color
static uint32_t parse_color(const char *str)
{
    if (strcmp(str, "red") == 0)
        return COLOR_RED;
    if (strcmp(str, "green") == 0)
        return COLOR_GREEN;
    if (strcmp(str, "blue") == 0)
        return COLOR_BLUE;
    if (strcmp(str, "white") == 0)
        return COLOR_WHITE;
    if (strcmp(str, "yellow") == 0)
        return COLOR_YELLOW;
    if (strcmp(str, "cyan") == 0)
        return COLOR_CYAN;
    if (strcmp(str, "magenta") == 0)
        return COLOR_MAGENTA;
    if (strcmp(str, "black") == 0)
        return COLOR_BLACK;
    return COLOR_WHITE; // default
}

static void cmd_cls(int argc, char **argv)
{
    clear_screen(COLOR_BLACK);
    print_str("Graphics screen cleared\n");
}

static void cmd_heap(int argc, char **argv)
{
    uint32_t total, used, free_mem;
    heap_stats(&total, &used, &free_mem);

    if (total == 0)
    {
        print_str("Heap not initialized\n");
        return;
    }

    print_str("Heap Statistics:\n");
    print_str("  Total: ");
    print_uint(total / 1024);
    print_str(" KB\n");

    print_str("  Used:  ");
    print_uint(used / 1024);
    print_str(" KB (");
    print_uint((used * 100) / total);
    print_str("%)\n");

    print_str("  Free:  ");
    print_uint(free_mem / 1024);
    print_str(" KB (");
    print_uint((free_mem * 100) / total);
    print_str("%)\n");
}

static void cmd_draw(int argc, char **argv)
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

    if (strcmp(argv[1], "triangle") == 0)
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
        int isFilled = 0;

        if (argc >= 10 && strcmp(argv[9], "filled") == 0)
        {
            isFilled = 1;
        }

        draw_triangle(x0, y0, x1, y1, x2, y2, color, isFilled);
        print_str("Triangle drawn!\n");
    }
    else if (strcmp(argv[1], "rect") == 0)
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
    else if (strcmp(argv[1], "line") == 0)
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
    else if (strcmp(argv[1], "pixel") == 0)
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
    else if (strcmp(argv[1], "circle") == 0)
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
        int isFilled = 0;

        if (argc >= 7 && strcmp(argv[6], "filled") == 0)
        {
            isFilled = 1;
        }

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

// Command implementations - all now take argc/argv
static void cmd_help(int argc, char **argv)
{
    print_str("Available commands:\n");
    for (int i = 0; i < MAX_COMMANDS && commands[i].name != NULL; i++)
    {
        print_str("  ");
        print_str(commands[i].name);
        print_str(" - ");
        print_str(commands[i].description);
        print_str("\n");
    }
}

static void cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/";

    vfs_node_t *dir = vfs_resolve_path(path);
    if (!dir)
    {
        print_str("Directory not found\n");
        return;
    }

    if (!(dir->flags & VFS_DIRECTORY))
    {
        print_str("Not a directory\n");
        return;
    }

    print_str("Contents of ");
    print_str(path);
    print_str(":\n");

    if (!dir->readdir)
    {
        print_str("Error: Directory listing not supported\n");
        return;
    }

    uint32_t i = 0;
    vfs_node_t *child;
    int file_count = 0;

    while (i < 64 && (child = vfs_readdir(dir, i)) != NULL)
    {
        print_str("  ");
        if (child->flags & VFS_DIRECTORY)
            print_str("[DIR]  ");
        else
            print_str("[FILE] ");
        print_str(child->name);
        print_str("\n");

        kfree(child);
        file_count++;
        i++;
    }

    if (file_count == 0)
    {
        print_str("  (empty)\n");
    }
    else
    {
        print_str("\nTotal: ");
        print_int(file_count);
        print_str(" item(s)\n");
    }
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: cat <file>\n");
        return;
    }

    vfs_node_t *file = vfs_open(argv[1], VFS_READ);
    if (!file)
    {
        print_str("File not found\n");
        return;
    }

    uint8_t buffer[256];
    int bytes = vfs_read(file, 0, 256, buffer);

    if (bytes > 0)
    {
        for (int i = 0; i < bytes; i++)
        {
            print_char(buffer[i]);
        }
    }

    vfs_close(file);
}

static void cmd_clear(int argc, char **argv)
{
    print_clear();
}

static void cmd_info(int argc, char **argv)
{
    print_str("Simple OS Shell\n");
    print_str("Version 0.1\n");
    print_str("System is running!\n");
}

static void cmd_reboot(int argc, char **argv)
{
    print_str("Rebooting system...\n");
    uint8_t good = 0x02;
    while (good & 0x02)
    {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

static void cmd_touch(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: touch <filename>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root)
    {
        print_str("Error: No filesystem mounted\n");
        return;
    }

    // Check if file already exists
    vfs_node_t *existing = vfs_finddir(root, argv[1]);
    if (existing)
    {
        print_str("File already exists: ");
        print_str(argv[1]);
        print_str("\n");
        kfree(existing);
        return;
    }

    // Create the file
    if (root->create)
    {
        int result = root->create(root, argv[1], VFS_FILE);
        if (result == 0)
        {
            print_str("Created: ");
            print_str(argv[1]);
            print_str("\n");
        }
        else
        {
            print_str("Error creating file\n");
        }
    }
    else
    {
        print_str("Error: Filesystem does not support file creation\n");
    }
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: rm <filename>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root)
    {
        print_str("Error: No filesystem mounted\n");
        return;
    }

    // Check if file exists
    vfs_node_t *existing = vfs_finddir(root, argv[1]);
    if (!existing)
    {
        print_str("File not found: ");
        print_str(argv[1]);
        print_str("\n");
        return;
    }
    kfree(existing);

    // Delete the file
    if (root->delete)
    {
        int result = root->delete(root, argv[1]);
        if (result == 0)
        {
            print_str("Removed: ");
            print_str(argv[1]);
            print_str("\n");
        }
        else
        {
            print_str("Error removing file\n");
        }
    }
    else
    {
        print_str("Error: Filesystem does not support file deletion\n");
    }
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 3)
    {
        print_str("Usage: write <filename> <text...>\n");
        return;
    }

    vfs_node_t *file = vfs_open(argv[1], VFS_WRITE);
    if (!file)
    {
        print_str("File not found: ");
        print_str(argv[1]);
        print_str("\nTip: Use 'touch' to create a file first\n");
        return;
    }

    // Concatenate all remaining arguments as the content
    char content[512];
    int pos = 0;
    for (int i = 2; i < argc && pos < 500; i++)
    {
        char *arg = argv[i];
        while (*arg && pos < 500)
        {
            content[pos++] = *arg++;
        }
        if (i < argc - 1 && pos < 500)
        {
            content[pos++] = ' ';
        }
    }
    content[pos] = '\0';

    int bytes = vfs_write(file, 0, pos, (uint8_t *)content);
    vfs_close(file);

    if (bytes > 0)
    {
        print_str("Wrote ");
        print_int(bytes);
        print_str(" bytes to ");
        print_str(argv[1]);
        print_str("\n");
    }
    else
    {
        print_str("Error writing to file\n");
    }
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        print_str(argv[i]);
        if (i < argc - 1)
        {
            print_str(" ");
        }
    }
    print_str("\n");
}

// Helper to parse IP address like "10.0.2.2" -> uint32_t
// Returns 0 if invalid (not a valid IP format)
static uint32_t parse_ip(const char *str) {
    if (!str || !*str) return 0;

    uint8_t octets[4] = {0};
    int octet_idx = 0;
    int value = 0;
    int has_digit = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            value = value * 10 + (*str - '0');
            has_digit = 1;
            if (value > 255) return 0;  // Invalid octet
        } else if (*str == '.') {
            if (!has_digit || octet_idx >= 3) return 0;  // Invalid format
            octets[octet_idx++] = value;
            value = 0;
            has_digit = 0;
        } else {
            // Invalid character (not a digit or dot) - likely a hostname
            return 0;
        }
        str++;
    }

    // Store last octet
    if (!has_digit || octet_idx != 3) return 0;  // Need exactly 4 octets
    octets[octet_idx] = value;

    return ip_to_uint32(octets[0], octets[1], octets[2], octets[3]);
}

static void cmd_ping(int argc, char **argv)
{
    if (argc < 2) {
        print_str("Usage: ping <ip address>\n");
        print_str("Example: ping 10.0.2.2\n");
        return;
    }

    uint32_t ip = parse_ip(argv[1]);
    if (ip == 0) {
        print_str("Error: Invalid IP address '");
        print_str(argv[1]);
        print_str("'\n");
        print_str("DNS is not supported. Use IP address (e.g., 10.0.2.2)\n");
        return;
    }

    print_str("Pinging ");
    print_str(argv[1]);
    print_str("...\n");

    int replies = ping(ip, 4);
    print_str("Received ");
    print_int(replies);
    print_str(" of 4 replies\n");
}

static void cmd_ifconfig(int argc, char **argv)
{
    // If arguments provided, set IP and/or gateway
    if (argc >= 2) {
        uint32_t new_ip = parse_ip(argv[1]);
        if (new_ip == 0) {
            print_str("Error: Invalid IP address '");
            print_str(argv[1]);
            print_str("'\n");
            return;
        }
        net_set_ip(new_ip);
        print_str("IP address set to ");
        print_str(argv[1]);
        print_str("\n");

        if (argc >= 3) {
            uint32_t new_gw = parse_ip(argv[2]);
            if (new_gw == 0) {
                print_str("Error: Invalid gateway address '");
                print_str(argv[2]);
                print_str("'\n");
                return;
            }
            net_set_gateway(new_gw);
            print_str("Gateway set to ");
            print_str(argv[2]);
            print_str("\n");
        }
        return;
    }

    // Show current configuration
    uint8_t mac[6];
    e1000_get_mac_address(mac);

    print_str("Network Configuration:\n");
    print_str("  MAC Address: ");
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 16) print_str("0");
        print_hex(mac[i]);
        if (i < 5) print_str(":");
    }
    print_str("\n");

    uint32_t ip = net_get_ip();
    uint8_t ip_bytes[4];
    uint32_to_ip(ip, ip_bytes);
    print_str("  IP Address:  ");
    print_int(ip_bytes[0]); print_str(".");
    print_int(ip_bytes[1]); print_str(".");
    print_int(ip_bytes[2]); print_str(".");
    print_int(ip_bytes[3]); print_str("\n");

    uint32_t gw = net_get_gateway();
    uint32_to_ip(gw, ip_bytes);
    print_str("  Gateway:     ");
    print_int(ip_bytes[0]); print_str(".");
    print_int(ip_bytes[1]); print_str(".");
    print_int(ip_bytes[2]); print_str(".");
    print_int(ip_bytes[3]); print_str("\n");

    print_str("  Link Status: ");
    print_str(e1000_link_up() ? "UP\n" : "DOWN\n");

    print_str("\nUsage: ifconfig [ip] [gateway]\n");
    print_str("  Example: ifconfig 192.168.1.100 192.168.1.1\n");
}

static void cmd_pci(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int count = pci_get_device_count();
    print_str("PCI Devices (");
    print_int(count);
    print_str(" found):\n");

    for (int i = 0; i < count; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev) continue;

        print_str("  ");
        print_int(dev->bus);
        print_str(":");
        print_int(dev->device);
        print_str(".");
        print_int(dev->function);
        print_str(" - Vendor: 0x");
        print_hex(dev->vendor_id);
        print_str(" Device: 0x");
        print_hex(dev->device_id);
        print_str(" Class: ");
        print_hex(dev->class_code);
        print_str(":");
        print_hex(dev->subclass);
        print_str("\n");
    }
}

static void cmd_netstat(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    print_str("Network Status:\n");

    // Show MMIO base address
    uint64_t mmio = e1000_get_mmio_base();
    print_str("  MMIO Base: 0x");
    // Print high 32 bits if non-zero
    if (mmio >> 32) {
        print_hex((mmio >> 32) & 0xFFFFFFFF);
    }
    print_hex(mmio & 0xFFFFFFFF);
    print_str("\n");

    // Show status register
    uint32_t status = e1000_get_status();
    print_str("  Status Reg: 0x");
    print_hex(status);
    print_str("\n");

    print_str("  Link: ");
    if (status & 0x02) {
        print_str("UP");
    } else {
        print_str("DOWN");
    }
    print_str(", Speed: ");
    uint32_t speed = (status >> 6) & 0x03;
    if (speed == 0) print_str("10 Mbps");
    else if (speed == 1) print_str("100 Mbps");
    else print_str("1000 Mbps");
    print_str("\n");

    // Show TX debug info
    e1000_debug_tx();

    // Process any pending packets
    print_str("Processing packets...\n");
    for (int i = 0; i < 10; i++) {
        net_process_packet();
    }
    print_str("Done.\n");
}

static void cmd_wget(int argc, char **argv)
{
    if (argc < 4) {
        print_str("Usage: wget <ip> <port> <path> [output_file]\n");
        print_str("Example: wget 192.168.1.100 80 /image.bmp /tmp/image.bmp\n");
        return;
    }

    uint32_t ip = parse_ip(argv[1]);
    if (ip == 0) {
        print_str("Error: Invalid IP address '");
        print_str(argv[1]);
        print_str("'\n");
        return;
    }

    int port = 0;
    const char *p = argv[2];
    while (*p >= '0' && *p <= '9') {
        port = port * 10 + (*p - '0');
        p++;
    }
    if (port <= 0 || port > 65535) {
        print_str("Error: Invalid port number\n");
        return;
    }

    print_str("Connecting to ");
    print_str(argv[1]);
    print_str(":");
    print_int(port);
    print_str(argv[3]);
    print_str("...\n");

    // Use larger buffer for binary files (512KB max to be safe)
    #define WGET_BUFFER_SIZE (512 * 1024)
    uint8_t *response = (uint8_t *)kmalloc(WGET_BUFFER_SIZE);
    if (!response) {
        print_str("Error: Out of memory\n");
        return;
    }

    int len = http_get(argv[1], (uint16_t)port, argv[3], (char *)response, WGET_BUFFER_SIZE);

    if (len > 0) {
        // Find end of HTTP headers (\r\n\r\n)
        uint8_t *body = response;
        int body_len = len;
        for (int i = 0; i < len - 3; i++) {
            if (response[i] == '\r' && response[i+1] == '\n' &&
                response[i+2] == '\r' && response[i+3] == '\n') {
                body = response + i + 4;
                body_len = len - (i + 4);
                break;
            }
        }

        print_str("Received ");
        print_int(body_len);
        print_str(" bytes\n");

        // If output file specified, save it
        if (argc >= 5) {
            // Use body (without headers) for saving
            uint8_t *save_data = body;
            int save_len = body_len;
            // Get filename from path (skip leading /)
            const char *filename = argv[4];
            if (filename[0] == '/') filename++;

            vfs_node_t *root = vfs_get_root();
            if (!root) {
                print_str("Error: No filesystem\n");
            } else {
                // Create file if it doesn't exist
                vfs_node_t *file = vfs_finddir(root, filename);
                if (!file && root->create) {
                    root->create(root, filename, VFS_FILE);
                    file = vfs_finddir(root, filename);
                }

                if (file) {
                    vfs_write(file, 0, save_len, save_data);
                    vfs_close(file);
                    print_str("Saved to ");
                    print_str(argv[4]);
                    print_str("\n");
                    kfree(file);
                } else {
                    print_str("Error: Could not create file\n");
                }
            }
        } else {
            // Print first 500 chars for text
            for (int i = 0; i < len && i < 500; i++) {
                if (response[i] >= 32 && response[i] < 127) {
                    print_char(response[i]);
                } else if (response[i] == '\n' || response[i] == '\r') {
                    print_char(response[i]);
                }
            }
            if (len > 500) {
                print_str("\n... (truncated)\n");
            }
        }
    } else {
        print_str("Failed to fetch URL (error ");
        print_int(len);
        print_str(")\n");
    }

    kfree(response);
}

static void cmd_view(int argc, char **argv)
{
    if (argc < 2) {
        print_str("Usage: view <image.bmp>\n");
        return;
    }

    print_str("Opening image: ");
    print_str(argv[1]);
    print_str("\n");

    print_str("Creating viewer window...\n");
    image_viewer_t *viewer = imageviewer_create("Image Viewer", 100, 100);
    if (!viewer) {
        print_str("Failed to create viewer window\n");
        return;
    }
    print_str("Window created.\n");

    print_str("Loading image file...\n");
    if (imageviewer_load_file(viewer, argv[1]) != 0) {
        print_str("Failed to load image: ");
        print_str(argv[1]);
        print_str("\n");
        imageviewer_close(viewer);
        return;
    }

    print_str("Image loaded successfully!\n");
    print_str("Drag the title bar to move. Click X to close.\n");
}

static void cmd_run(int argc, char **argv)
{
    if (argc < 2) {
        print_str("Usage: run <program>\n");
        return;
    }

    print_str("Running: ");
    print_str(argv[1]);
    print_str("\n");

    int ret = process_exec_file(argv[1]);
    if (ret < 0) {
        print_str("Failed to execute program (error ");
        print_int(-ret);
        print_str(")\n");
    } else {
        print_str("Program exited with code ");
        print_int(ret);
        print_str("\n");
    }
}

static void cmd_gui(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    print_str("Launching Desktop Environment...\n");

    // Initialize desktop
    desktop_init();

    print_str("Desktop ready!\n");
    print_str("Click taskbar buttons to open apps.\n");
    print_str("Drag title bars to move windows.\n");
    print_str("Click X to close windows.\n");
}

static void cmd_edit(int argc, char **argv)
{
    // Make sure desktop is active first
    if (!desktop_is_active()) {
        desktop_init();
    }

    // Open text editor via desktop
    desktop_open_app(APP_EDITOR);

    print_str("Text editor opened!\n");
    (void)argc;
    (void)argv;
}

// Initialize the shell
void shell_init(void)
{
    reset_command_buffer();
    clear_screen(COLOR_BLACK);
    print_str("Shell initialized\n");
}

// Process a single character of input
void shell_process_char(char c)
{
    if (c == '\n')
    {
        // Execute the command on Enter
        print_str("\n");
        execute_command();
        reset_command_buffer();
        print_str(PROMPT);
    }
    else if (c == '\b')
    {
        // Handle backspace
        if (buffer_pos > 0)
        {
            buffer_pos--;
            // Move cursor back, print space, move cursor back again
            print_str("\b \b");
        }
    }
    else if (buffer_pos < MAX_COMMAND_LENGTH - 1)
    {
        // Add character to buffer
        command_buffer[buffer_pos++] = c;
    }
}

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putchar(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, c);
}

void serial_print(const char *str)
{
    while (*str)
    {
        serial_putchar(*str++);
    }
}

void serial_print_hex(uint64_t value)
{
    char hex[] = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4)
    {
        serial_putchar(hex[(value >> i) & 0xF]);
    }
}

void serial_print_dec(uint32_t value)
{
    char buffer[32];
    int i = 0;

    if (value == 0)
    {
        serial_putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        serial_putchar(buffer[--i]);
    }
}

// Main shell loop
void shell_run(void)
{
    shell_init();
    print_str(PROMPT);

    while (1)
    {
        if (keyboard_available())
        {
            char c = keyboard_read();

            // Route keyboard to desktop if active
            int handled = 0;
            if (desktop_is_active()) {
                handled = desktop_handle_key(c);
            }

            // If not handled by desktop, process in shell
            if (!handled) {
                shell_process_char(c);
                if (c >= 32 && c != 127) // printable ASCII only
                {
                    print_char(c);
                }
            }
        }

        static int32_t last_x = -1;
        static int32_t last_y = -1;
        static uint8_t last_buttons = 0;

        // Handle mouse
        mouse_state_t mouse = mouse_get_state();

        // Update if position OR buttons changed
        if (mouse.x != last_x || mouse.y != last_y || mouse.buttons != last_buttons)
        {
            // Let desktop handle mouse first (for taskbar)
            int desktop_handled = 0;
            if (desktop_is_active()) {
                desktop_handled = desktop_handle_mouse(mouse.x, mouse.y, mouse.buttons);
            }

            // Let window manager handle if desktop didn't consume it
            if (!desktop_handled) {
                wm_handle_mouse(mouse.x, mouse.y, mouse.buttons);
            }

            // Update cursor state based on context
            if (wm_is_dragging()) {
                set_cursor_state(CURSOR_MOVE);
                set_cursor_color(0xFFFFFF);
            } else {
                // Check if hovering over a window title bar
                window_t *win = wm_get_window_at(mouse.x, mouse.y);
                if (win && mouse.y < win->y + WINDOW_TITLE_HEIGHT && mouse.y >= win->y) {
                    set_cursor_state(CURSOR_HAND);
                } else {
                    set_cursor_state(CURSOR_ARROW);
                }
                set_cursor_color(0xFFFFFF);
            }

            cursor_update(mouse.x, mouse.y);
            last_x = mouse.x;
            last_y = mouse.y;
            last_buttons = mouse.buttons;
        }

        // Render windows (only if something changed)
        wm_render();

        // Process network packets
        net_process_packet();

        // // Debug: show interrupt count occasionally
        // extern volatile uint32_t mouse_interrupt_count;
        // static uint32_t last_count = 0;
        // static int tick = 0;
        // if (++tick > 100000)
        // {
        //     tick = 0;
        //     if (mouse_interrupt_count != last_count)
        //     {
        //         serial_print("IRQ count: ");
        //         serial_print_dec(mouse_interrupt_count);
        //         serial_print("\n");
        //         last_count = mouse_interrupt_count;
        //     }
        // }
    }
}