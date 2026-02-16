
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
#include <utils/log.h>
#include <utils/timer.h>

void kernel_main(void) {
    log_init();
    timer_init();

    uint64_t t, boot_start = timer_ticks();

    t = timer_ticks();
    init_interrupts_safe();
    LOG_INFO("BOOT", "Interrupts: %u ms", timer_ms_since(t));

    t = timer_ticks();
    paging_init();
    LOG_INFO("BOOT", "Paging: %u ms", timer_ms_since(t));

    t = timer_ticks();
    keyboard_init();
    LOG_INFO("BOOT", "Keyboard: %u ms", timer_ms_since(t));

    t = timer_ticks();
    mouse_init();
    LOG_INFO("BOOT", "Mouse: %u ms", timer_ms_since(t));

    t = timer_ticks();
    ata_init();
    LOG_INFO("BOOT", "ATA: %u ms", timer_ms_since(t));

    t = timer_ticks();
    kernel_filesystem_init();
    simplefs_create_sample_files();
    LOG_INFO("BOOT", "Filesystem: %u ms", timer_ms_since(t));

    t = timer_ticks();
    pci_init();
    int net_status = e1000_init();
    if (net_status == 0) {
        net_init();
        socket_init();
    }
    LOG_INFO("BOOT", "Network: %u ms", timer_ms_since(t));

    t = timer_ticks();
    process_init();
    syscall_init();
    LOG_INFO("BOOT", "Process/syscall: %u ms", timer_ms_since(t));

    enable_interrupts();

    t = timer_ticks();
    graphics_init();
    print_init();
    cursor_init();
    cursor_show();
    LOG_INFO("BOOT", "Graphics: %u ms", timer_ms_since(t));

    t = timer_ticks();
    wm_init();
    LOG_INFO("BOOT", "Window manager: %u ms", timer_ms_since(t));

    // Draw a welcome screen
    uint32_t cx = get_screen_width() / 2;
    uint32_t cy = get_screen_height() / 2;

    draw_triangle(cx, cy - 100, cx - 100, cy + 100, cx + 100, cy + 100, COLOR_CYAN, 1);
    fill_rect(10, 10, 300, 50, COLOR_BLUE);
    draw_string("Network Manager OS", 20, 20, COLOR_WHITE, COLOR_BLUE);
    draw_string("Type 'help' for commands", 20, 35, COLOR_YELLOW, COLOR_BLUE);

    LOG_INFO("BOOT", "Total boot time: %u ms", timer_ms_since(boot_start));
    LOG_INFO("BOOT", "BOOT_COMPLETE");

    shell_run();
}
