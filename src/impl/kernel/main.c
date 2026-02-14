
#include <interrupts/safeInterrupt.h>
#include <interrupts/io/keyboard.h>
#include <interrupts/io/mouse.h>
#include <interrupts/io/ata.h>

#include <graphics/graphics.h>
#include <graphics/cursor.h>
#include <graphics/font.h>

#include <shell/print.h>
#include <shell/shell.h>

#include <disk/partition.h>
#include <disk/partition_block_device.h>
#include <disk/test.h>

#include <fs/vfs.h>
#include <fs/vfs_mount.h>
#include <fs/simplefs.h>

#include <memory/heap.h>
#include <memory/paging.h>

#include <init/inits.h>

// New includes for network manager
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <net/net.h>
#include <net/socket.h>
#include <exec/process.h>
#include <exec/syscall.h>
#include <gui/window.h>

void kernel_main(void)
{
    init_interrupts_safe();

    // Detect physical memory and allocate page tables
    paging_init();

    // Initialize input devices
    keyboard_init();
    mouse_init();
    ata_init();

    // Initialize filesystem
    kernel_filesystem_init();
    simplefs_create_sample_files();

    // Initialize PCI and network
    pci_init();
    int net_status = e1000_init();
    if (net_status == 0) {
        net_init();
        socket_init();
    }

    // Initialize process management and syscalls
    process_init();
    syscall_init();

    enable_interrupts();

    // Initialize graphics
    graphics_init();
    print_init();
    cursor_init();
    cursor_show();

    // Initialize window manager
    wm_init();

    // Draw a welcome screen
    uint32_t cx = get_screen_width() / 2;
    uint32_t cy = get_screen_height() / 2;

    draw_triangle(cx, cy - 100, cx - 100, cy + 100, cx + 100, cy + 100, COLOR_CYAN, 1);
    fill_rect(10, 10, 300, 50, COLOR_BLUE);
    draw_string("Network Manager OS", 20, 20, COLOR_WHITE, COLOR_BLUE);
    draw_string("Type 'help' for commands", 20, 35, COLOR_YELLOW, COLOR_BLUE);

    shell_run();
}