#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

// File types
#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02
#define VFS_CHARDEVICE 0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE 0x05
#define VFS_SYMLINK 0x06
#define VFS_MOUNTPOINT 0x08

// File permissions/flags
#define VFS_READ 0x01
#define VFS_WRITE 0x02
#define VFS_APPEND 0x04
#define VFS_CREATE 0x08

// Forward declaration
typedef struct vfs_node vfs_node_t;

// Function pointer types for VFS operations
typedef int (*vfs_read_fn)(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef int (*vfs_write_fn)(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef void (*vfs_open_fn)(vfs_node_t *node, uint32_t flags);
typedef void (*vfs_close_fn)(vfs_node_t *node);
typedef vfs_node_t *(*vfs_readdir_fn)(vfs_node_t *node, uint32_t index);
typedef vfs_node_t *(*vfs_finddir_fn)(vfs_node_t *node, const char *name);
typedef int (*vfs_create_fn)(vfs_node_t *parent, const char *name, uint32_t flags);
typedef int (*vfs_delete_fn)(vfs_node_t *node);

// VFS Node structure
struct vfs_node
{
    char name[256];  // File/directory name
    uint32_t flags;  // Type and permissions
    uint32_t inode;  // Inode number
    uint32_t length; // File size in bytes
    uint32_t impl;   // Implementation-specific data

    vfs_read_fn read;       // Read function
    vfs_write_fn write;     // Write function
    vfs_open_fn open;       // Open function
    vfs_close_fn close;     // Close function
    vfs_readdir_fn readdir; // Read directory entry
    vfs_finddir_fn finddir; // Find child by name
    vfs_create_fn create;   // Create new file/dir
    vfs_delete_fn delete;   // Delete file/dir

    vfs_node_t *parent; // Parent directory
    void *filesystem;   // Pointer to filesystem-specific data
};

// VFS API functions
void vfs_init(void);
vfs_node_t *vfs_get_root(void);
vfs_node_t *vfs_open(const char *path, uint32_t flags);
void vfs_close(vfs_node_t *node);
int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
vfs_node_t *vfs_readdir(vfs_node_t *node, uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
int vfs_create(const char *path, uint32_t flags);
int vfs_delete(const char *path);
int vfs_mount(const char *device, const char *mountpoint);

// Helper functions
vfs_node_t *vfs_resolve_path(const char *path);
const char *vfs_get_filename(const char *path);

#endif