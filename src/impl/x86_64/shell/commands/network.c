// Network commands: ping, ifconfig, netstat, wget

#include <shell/commands.h>
#include <shell/print.h>
#include <drivers/e1000.h>
#include <net/net.h>
#include <net/socket.h>
#include <memory/heap.h>
#include <fs/vfs.h>

// Helper to parse IP address
static uint32_t parse_ip(const char *str)
{
    if (!str || !*str)
        return 0;

    uint8_t octets[4] = {0};
    int octet_idx = 0;
    int value = 0;
    int has_digit = 0;

    while (*str)
    {
        if (*str >= '0' && *str <= '9')
        {
            value = value * 10 + (*str - '0');
            has_digit = 1;
            if (value > 255)
                return 0;
        }
        else if (*str == '.')
        {
            if (!has_digit || octet_idx >= 3)
                return 0;
            octets[octet_idx++] = value;
            value = 0;
            has_digit = 0;
        }
        else
        {
            return 0;
        }
        str++;
    }

    if (!has_digit || octet_idx != 3)
        return 0;
    octets[octet_idx] = value;

    return ip_to_uint32(octets[0], octets[1], octets[2], octets[3]);
}

void cmd_ping(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: ping <ip address>\n");
        print_str("Example: ping 10.0.2.2\n");
        return;
    }

    uint32_t ip = parse_ip(argv[1]);
    if (ip == 0)
    {
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

void cmd_ifconfig(int argc, char **argv)
{
    if (argc >= 2)
    {
        uint32_t new_ip = parse_ip(argv[1]);
        if (new_ip == 0)
        {
            print_str("Error: Invalid IP address '");
            print_str(argv[1]);
            print_str("'\n");
            return;
        }
        net_set_ip(new_ip);
        print_str("IP address set to ");
        print_str(argv[1]);
        print_str("\n");

        if (argc >= 3)
        {
            uint32_t new_gw = parse_ip(argv[2]);
            if (new_gw == 0)
            {
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

    uint8_t mac[6];
    e1000_get_mac_address(mac);

    print_str("Network Configuration:\n");
    print_str("  MAC Address: ");
    for (int i = 0; i < 6; i++)
    {
        if (mac[i] < 16)
            print_str("0");
        print_hex(mac[i]);
        if (i < 5)
            print_str(":");
    }
    print_str("\n");

    uint32_t ip = net_get_ip();
    uint8_t ip_bytes[4];
    uint32_to_ip(ip, ip_bytes);
    print_str("  IP Address:  ");
    print_int(ip_bytes[0]);
    print_str(".");
    print_int(ip_bytes[1]);
    print_str(".");
    print_int(ip_bytes[2]);
    print_str(".");
    print_int(ip_bytes[3]);
    print_str("\n");

    uint32_t gw = net_get_gateway();
    uint32_to_ip(gw, ip_bytes);
    print_str("  Gateway:     ");
    print_int(ip_bytes[0]);
    print_str(".");
    print_int(ip_bytes[1]);
    print_str(".");
    print_int(ip_bytes[2]);
    print_str(".");
    print_int(ip_bytes[3]);
    print_str("\n");

    print_str("  Link Status: ");
    print_str(e1000_link_up() ? "UP\n" : "DOWN\n");

    print_str("\nUsage: ifconfig [ip] [gateway]\n");
    print_str("  Example: ifconfig 192.168.1.100 192.168.1.1\n");
}

void cmd_netstat(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    print_str("Network Status:\n");

    uint64_t mmio = e1000_get_mmio_base();
    print_str("  MMIO Base: 0x");
    if (mmio >> 32)
    {
        print_hex((mmio >> 32) & 0xFFFFFFFF);
    }
    print_hex(mmio & 0xFFFFFFFF);
    print_str("\n");

    uint32_t status = e1000_get_status();
    print_str("  Status Reg: 0x");
    print_hex(status);
    print_str("\n");

    print_str("  Link: ");
    if (status & 0x02)
    {
        print_str("UP");
    }
    else
    {
        print_str("DOWN");
    }
    print_str(", Speed: ");
    uint32_t speed = (status >> 6) & 0x03;
    if (speed == 0)
        print_str("10 Mbps");
    else if (speed == 1)
        print_str("100 Mbps");
    else
        print_str("1000 Mbps");
    print_str("\n");

    e1000_debug_tx();

    print_str("Processing packets...\n");
    for (int i = 0; i < 10; i++)
    {
        net_process_packet();
    }
    print_str("Done.\n");
}

void cmd_wget(int argc, char **argv)
{
    if (argc < 4)
    {
        print_str("Usage: wget <ip> <port> <path> [output_file]\n");
        print_str("Example: wget 192.168.1.100 80 /image.bmp /tmp/image.bmp\n");
        return;
    }

    uint32_t ip = parse_ip(argv[1]);
    if (ip == 0)
    {
        print_str("Error: Invalid IP address '");
        print_str(argv[1]);
        print_str("'\n");
        return;
    }

    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535)
    {
        print_str("Error: Invalid port number\n");
        return;
    }

    print_str("Connecting to ");
    print_str(argv[1]);
    print_str(":");
    print_int(port);
    print_str(argv[3]);
    print_str("...\n");

#define WGET_BUFFER_SIZE (512 * 1024)
    uint8_t *response = (uint8_t *)kmalloc(WGET_BUFFER_SIZE);
    if (!response)
    {
        print_str("Error: Out of memory\n");
        return;
    }

    int len = http_get(argv[1], (uint16_t)port, argv[3], (char *)response, WGET_BUFFER_SIZE);

    if (len > 0)
    {
        uint8_t *body = response;
        int body_len = len;
        for (int i = 0; i < len - 3; i++)
        {
            if (response[i] == '\r' && response[i + 1] == '\n' &&
                response[i + 2] == '\r' && response[i + 3] == '\n')
            {
                body = response + i + 4;
                body_len = len - (i + 4);
                break;
            }
        }

        print_str("Received ");
        print_int(body_len);
        print_str(" bytes\n");

        if (argc >= 5)
        {
            const char *filename = argv[4];
            if (filename[0] == '/')
                filename++;

            vfs_node_t *root = vfs_get_root();
            if (!root)
            {
                print_str("Error: No filesystem\n");
            }
            else
            {
                vfs_node_t *file = vfs_finddir(root, filename);
                if (!file && root->create)
                {
                    root->create(root, filename, VFS_FILE);
                    file = vfs_finddir(root, filename);
                }

                if (file)
                {
                    vfs_write(file, 0, body_len, body);
                    vfs_close(file);
                    print_str("Saved to ");
                    print_str(argv[4]);
                    print_str("\n");
                    kfree(file);
                }
                else
                {
                    print_str("Error: Could not create file\n");
                }
            }
        }
        else
        {
            for (int i = 0; i < len && i < 500; i++)
            {
                if (response[i] >= 32 && response[i] < 127)
                {
                    print_char(response[i]);
                }
                else if (response[i] == '\n' || response[i] == '\r')
                {
                    print_char(response[i]);
                }
            }
            if (len > 500)
            {
                print_str("\n... (truncated)\n");
            }
        }
    }
    else
    {
        print_str("Failed to fetch URL (error ");
        print_int(len);
        print_str(")\n");
    }

    kfree(response);
}
