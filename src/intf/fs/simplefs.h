// #pragma once
// #include <stdint.h>

// #define SIMPLEFS_DIRECT_BLOCKS 12

// #define SIMPLEFS_MAGIC 0x53465321 // "SFS!" -- SimpleFS magic number
// #define SIMPLEFS_VERSION 1

// typedef struct simplefs_header
// {
//     char signature[4]; // "SFS\0"
//     uint32_t version;
//     uint32_t inode_count;
//     uint32_t block_size;
// } simplefs_header_t;

// typedef struct simplefs_superblock
// {
//     uint32_t magic;   // 0x53465321 "SFS!" -- SimpleFS magic number
//     uint32_t version; // Filesystem version

//     // Block information (group related fields)
//     uint32_t block_size;       // Size of each block (512, 1024, 4096, etc.)
//     uint32_t total_blocks;     // Total blocks in filesystem
//     uint32_t free_block_count; // Number of free blocks
//     uint32_t first_data_block; // First block available for data

//     // Inode information
//     uint32_t inode_count;       // Total inodes
//     uint32_t free_inode_count;  // Free inodes
//     uint32_t inodetable_start;  // Starting block of inode table
//     uint32_t inodetable_blocks; // Number of blocks for inode table (renamed for clarity)

//     // Metadata
//     uint32_t mount_count;     // Times mounted (useful for fsck)
//     uint32_t last_mount_time; // Timestamp of last mount
//     uint32_t last_write_time; // Timestamp of last write

//     // Padding to align to cache line / block boundary
//     uint8_t reserved[512 - (13 * 4)];
// } __attribute__((packed)) simplefs_superblock_t;

// typedef struct simplefs_inode
// {
//     uint32_t file_size; // bytes
//     uint16_t mode;      // file type + permissions
//     uint16_t link_count;

//     uint32_t direct_blocks[SIMPLEFS_DIRECT_BLOCKS];
//     uint32_t indirect_block;

//     uint32_t ctime;
//     uint32_t mtime;
//     uint32_t atime;
// } __attribute__((packed)) simplefs_inode_t;

// typedef struct simplefs_dir_entry
// {
//     uint32_t inode_number;
//     uint16_t record_length; // total entry size
//     uint8_t name_length;
//     uint8_t file_type; // file / dir
//     char name[];
// } __attribute__((packed)) simplefs_dir_entry_t;

// // Initialize SimpleFS
// void simplefs_init(void);
// // Mount SimpleFS from a given device
// int simplefs_mount(uint32_t device_id);
// // Read a file from SimpleFS
// int simplefs_read_file(uint32_t inode_number, uint8_t *buffer, uint32_t size, uint32_t offset);
// // List directory contents
// int simplefs_list_dir(uint32_t inode_number);
// // Find a file in a directory
// int simplefs_find_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number);
// // Create a new file
// int simplefs_create_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number);
// // Delete a file
// int simplefs_delete_file(uint32_t dir_inode_number, const char *filename);
// // Write data to a file
// int simplefs_write_file(uint32_t inode_number, const uint8_t *buffer, uint32_t size, uint32_t offset);