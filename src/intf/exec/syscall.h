#pragma once
#include <stdint.h>
#include <stddef.h>

// Syscall numbers
#define SYS_EXIT        0
#define SYS_READ        1
#define SYS_WRITE       2
#define SYS_OPEN        3
#define SYS_CLOSE       4
#define SYS_GETPID      5
#define SYS_SOCKET      6
#define SYS_CONNECT     7
#define SYS_SEND        8
#define SYS_RECV        9
#define SYS_BIND        10
#define SYS_LISTEN      11
#define SYS_ACCEPT      12

// File descriptors
#define STDIN_FD   0
#define STDOUT_FD  1
#define STDERR_FD  2

// Maximum open files per process
#define MAX_OPEN_FILES 32

// File descriptor entry
typedef struct {
    uint32_t flags;
    void *node;       // VFS node or socket
    uint32_t offset;  // Current file offset
    uint8_t type;     // 0 = file, 1 = socket
} fd_entry_t;

// Syscall handler type
typedef int64_t (*syscall_handler_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Initialize syscall interface
void syscall_init(void);

// Syscall interrupt handler (called from assembly)
int64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5);

// Individual syscall implementations
int64_t sys_exit(int64_t code);
int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_open(const char *path, int flags);
int64_t sys_close(int fd);
int64_t sys_getpid(void);
