#include "exec/syscall.h"
#include "exec/process.h"
#include "fs/vfs.h"
#include "shell/print.h"
#include "interrupts/idt.h"
#include "utils/memory.h"

// File descriptor table for current process (simplified - global for now)
static fd_entry_t fd_table[MAX_OPEN_FILES];
static int fd_initialized = 0;

// Forward declaration of assembly handler
extern void syscall_interrupt_handler(void);

static void init_fd_table(void) {
    if (fd_initialized) return;

    memset(fd_table, 0, sizeof(fd_table));

    // Set up stdin, stdout, stderr as special console descriptors
    fd_table[STDIN_FD].flags = 1;   // Open
    fd_table[STDIN_FD].type = 0;
    fd_table[STDIN_FD].node = 0;    // Console, no VFS node

    fd_table[STDOUT_FD].flags = 1;
    fd_table[STDOUT_FD].type = 0;
    fd_table[STDOUT_FD].node = 0;

    fd_table[STDERR_FD].flags = 1;
    fd_table[STDERR_FD].type = 0;
    fd_table[STDERR_FD].node = 0;

    fd_initialized = 1;
}

void syscall_init(void) {
    // Register interrupt 0x80 for syscalls
    idt_set_gate(0x80, (uint64_t)syscall_interrupt_handler);

    init_fd_table();
}

// Main syscall dispatcher
int64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg4;
    (void)arg5;

    switch (num) {
        case SYS_EXIT:
            return sys_exit((int64_t)arg1);

        case SYS_READ:
            return sys_read((int)arg1, (void *)arg2, (size_t)arg3);

        case SYS_WRITE:
            return sys_write((int)arg1, (const void *)arg2, (size_t)arg3);

        case SYS_OPEN:
            return sys_open((const char *)arg1, (int)arg2);

        case SYS_CLOSE:
            return sys_close((int)arg1);

        case SYS_GETPID:
            return sys_getpid();

        default:
            return -1;  // Unknown syscall
    }
}

// Exit syscall
int64_t sys_exit(int64_t code) {
    process_exit((int)code);
    // Should not return
    return 0;
}

// Read syscall
int64_t sys_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }

    if (fd_table[fd].flags == 0) {
        return -1;  // Not open
    }

    if (fd == STDIN_FD) {
        // Read from keyboard - simplified version
        // For now, just return 0 (EOF)
        return 0;
    }

    vfs_node_t *node = (vfs_node_t *)fd_table[fd].node;
    if (!node) {
        return -1;
    }

    int bytes = vfs_read(node, fd_table[fd].offset, count, buf);
    if (bytes > 0) {
        fd_table[fd].offset += bytes;
    }

    return bytes;
}

// Write syscall
int64_t sys_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }

    if (fd_table[fd].flags == 0) {
        return -1;  // Not open
    }

    if (fd == STDOUT_FD || fd == STDERR_FD) {
        // Write to console
        const char *str = (const char *)buf;
        for (size_t i = 0; i < count; i++) {
            print_char(str[i]);
        }
        return count;
    }

    vfs_node_t *node = (vfs_node_t *)fd_table[fd].node;
    if (!node) {
        return -1;
    }

    int bytes = vfs_write(node, fd_table[fd].offset, count, (uint8_t *)buf);
    if (bytes > 0) {
        fd_table[fd].offset += bytes;
    }

    return bytes;
}

// Open syscall
int64_t sys_open(const char *path, int flags) {
    // Find free fd
    int fd = -1;
    for (int i = 3; i < MAX_OPEN_FILES; i++) {  // Start at 3, skip std*
        if (fd_table[i].flags == 0) {
            fd = i;
            break;
        }
    }

    if (fd < 0) {
        return -1;  // No free fd
    }

    // Open file
    vfs_node_t *node = vfs_open(path, flags);
    if (!node) {
        return -1;
    }

    fd_table[fd].flags = 1;
    fd_table[fd].node = node;
    fd_table[fd].offset = 0;
    fd_table[fd].type = 0;

    return fd;
}

// Close syscall
int64_t sys_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }

    if (fd_table[fd].flags == 0) {
        return -1;  // Not open
    }

    if (fd_table[fd].node) {
        vfs_close((vfs_node_t *)fd_table[fd].node);
    }

    fd_table[fd].flags = 0;
    fd_table[fd].node = 0;
    fd_table[fd].offset = 0;

    return 0;
}

// Getpid syscall
int64_t sys_getpid(void) {
    process_t *proc = process_get_current();
    if (proc) {
        return proc->pid;
    }
    return 0;
}
