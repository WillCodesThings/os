// #include <fs/simplefs.h>

// #include <fs/vfs.h>
// #include <fs/tmpfs.h>

// simplefs_superblock_t simplefs_superblock;

// vfs_node_t *simplefs_root = NULL;

// void simplefs_init(void)
// {
//     serial_print("Initializing SimpleFS.\n");
//     simplefs_superblock = (simplefs_superblock_t){
//         .magic = SIMPLEFS_MAGIC,
//         .version = SIMPLEFS_VERSION,
//         .block_size = 4096,
//         .total_blocks = 0,
//         .free_blocks = 0,
//         .total_inodes = 0,
//         .free_inodes = 0,
//         .last_mount_time = 0,
//         .last_write_time = 0,
//     };

//     vfs_mount("/simplefs_device", simplefs_root);
// }

// int simplefs_mount(uint32_t device_id)
// {
//     serial_print("Mounting SimpleFS (stub).\n");
//     return 0;
// }

// int simplefs_read_file(uint32_t inode_number, uint8_t *buffer, uint32_t size, uint32_t offset)
// {
//     serial_print("Reading file from SimpleFS (stub).\n");
//     return 0;
// }

// int simplefs_list_dir(uint32_t inode_number)
// {
//     serial_print("Listing directory in SimpleFS (stub).\n");
//     return 0;
// }

// int simplefs_find_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number)
// {
//     serial_print("Finding file in SimpleFS (stub).\n");
//     return -1;
// }

// int simplefs_create_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number)
// {
//     serial_print("Creating file in SimpleFS (stub).\n");
//     return -1;
// }

// int simplefs_delete_file(uint32_t dir_inode_number, const char *filename)
// {
//     serial_print("Deleting file in SimpleFS (stub).\n");
//     return -1;
// }

// int simplefs_write_file(uint32_t inode_number, const uint8_t *buffer, uint32_t size, uint32_t offset)
// {
//     serial_print("Writing file to SimpleFS (stub).\n");
//     return -1;
// }
