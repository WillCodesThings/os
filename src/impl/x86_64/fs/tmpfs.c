#include <fs/tmpfs.h>
#include <fs/vfs.h>
#include <shell/shell.h>
#include <memory/heap.h>

#define MAX_FILES 64
#define MAX_CHILDREN 64
#define MAX_FILE_SIZE (512 * 1024)  // 512KB max per file

typedef struct
{
    uint8_t *data;           // Dynamically allocated
    uint32_t size;
    uint32_t capacity;
    vfs_node_t *children[MAX_CHILDREN];
    uint32_t child_count;
} tmpfs_data_t;

static uint32_t next_inode = 1;

// Allocate a new tmpfs data structure
static tmpfs_data_t *tmpfs_alloc_data(void)
{
    tmpfs_data_t *data = (tmpfs_data_t *)kcalloc(1, sizeof(tmpfs_data_t));
    if (!data) return NULL;

    data->size = 0;
    data->capacity = 0;
    data->data = NULL;
    data->child_count = 0;

    return data;
}

// tmpfs read function
static int tmpfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !buffer)
        return -1;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data || !data->data)
        return -1;

    if (offset >= data->size)
        return 0;

    if (offset + size > data->size)
    {
        size = data->size - offset;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        buffer[i] = data->data[offset + i];
    }

    return size;
}

// tmpfs write function
static int tmpfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !buffer)
        return -1;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data)
        return -1;

    uint32_t needed = offset + size;
    if (needed > MAX_FILE_SIZE)
    {
        needed = MAX_FILE_SIZE;
        size = needed - offset;
    }

    // Allocate or expand buffer if needed
    if (needed > data->capacity)
    {
        uint32_t new_cap = needed + 4096;  // Add some extra
        if (new_cap > MAX_FILE_SIZE) new_cap = MAX_FILE_SIZE;

        uint8_t *new_data = (uint8_t *)kmalloc(new_cap);
        if (!new_data)
            return -1;

        // Copy existing data
        if (data->data && data->size > 0)
        {
            for (uint32_t i = 0; i < data->size; i++)
            {
                new_data[i] = data->data[i];
            }
            kfree(data->data);
        }

        data->data = new_data;
        data->capacity = new_cap;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        data->data[offset + i] = buffer[i];
    }

    if (offset + size > data->size)
    {
        data->size = offset + size;
        node->length = data->size;
    }

    return size;
}

// tmpfs readdir function
static vfs_node_t *tmpfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY))
        return NULL;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data || index >= data->child_count)
        return NULL;

    return data->children[index];
}

// tmpfs finddir function
static vfs_node_t *tmpfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY))
        return NULL;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data)
        return NULL;

    for (uint32_t i = 0; i < data->child_count; i++)
    {
        vfs_node_t *child = data->children[i];

        // Simple string comparison
        const char *n1 = child->name;
        const char *n2 = name;
        int match = 1;
        while (*n1 && *n2)
        {
            if (*n1++ != *n2++)
            {
                match = 0;
                break;
            }
        }
        if (match && *n1 == *n2)
        {
            return child;
        }
    }

    return NULL;
}

// Forward declaration
static int tmpfs_create(vfs_node_t *parent, const char *name, uint32_t flags);

// Create a new tmpfs node (internal version that returns the node)
vfs_node_t *tmpfs_create_file(vfs_node_t *parent, const char *name, uint32_t flags)
{
    if (!parent || !(parent->flags & VFS_DIRECTORY))
        return NULL;

    tmpfs_data_t *parent_data = (tmpfs_data_t *)parent->filesystem;
    if (!parent_data || parent_data->child_count >= MAX_CHILDREN)
        return NULL;

    // Allocate new node dynamically
    vfs_node_t *node = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!node)
        return NULL;

    // Copy name
    int i = 0;
    while (name[i] && i < 255)
    {
        node->name[i] = name[i];
        i++;
    }
    node->name[i] = '\0';

    node->flags = flags;
    node->inode = next_inode++;
    node->length = 0;
    node->parent = parent;

    // Allocate data
    node->filesystem = tmpfs_alloc_data();

    // Set function pointers
    node->read = tmpfs_read;
    node->write = tmpfs_write;
    node->open = NULL;
    node->close = NULL;
    node->readdir = tmpfs_readdir;
    node->finddir = tmpfs_finddir;
    node->create = tmpfs_create;

    // Add to parent
    parent_data->children[parent_data->child_count++] = node;

    return node;
}

// VFS-compatible create wrapper (returns int instead of node)
static int tmpfs_create(vfs_node_t *parent, const char *name, uint32_t flags)
{
    vfs_node_t *node = tmpfs_create_file(parent, name, flags);
    return node ? 0 : -1;
}

void tmpfs_init(void)
{
    serial_print("Initializing tmpfs...\n");

    // Create root directory
    static vfs_node_t root;
    root.name[0] = '/';
    root.name[1] = '\0';
    root.flags = VFS_DIRECTORY;
    root.inode = 0;
    root.length = 0;
    root.parent = NULL;
    root.filesystem = tmpfs_alloc_data();

    root.read = tmpfs_read;
    root.write = tmpfs_write;
    root.readdir = tmpfs_readdir;
    root.finddir = tmpfs_finddir;
    root.create = tmpfs_create;
    root.open = NULL;
    root.close = NULL;

    // Set as VFS root
    extern void vfs_set_root(vfs_node_t * root);
    vfs_set_root(&root);

    serial_print("tmpfs mounted as root\n");

    // Create some test files
    vfs_node_t *readme = tmpfs_create_file(&root, "readme.txt", VFS_FILE);
    if (readme)
    {
        const char *content = "Welcome to MyOS!\nThis is a test file.\n";
        int len = 0;
        while (content[len])
            len++;
        tmpfs_write(readme, 0, len, (uint8_t *)content);
        serial_print("Created /readme.txt\n");
    }

    // Create a directory
    vfs_node_t *docs = tmpfs_create_file(&root, "docs", VFS_DIRECTORY);
    if (docs)
    {
        serial_print("Created /docs directory\n");

        // Create file in docs
        vfs_node_t *test = tmpfs_create_file(docs, "test.txt", VFS_FILE);
        if (test)
        {
            const char *content = "Test file in docs directory\n";
            int len = 0;
            while (content[len])
                len++;
            tmpfs_write(test, 0, len, (uint8_t *)content);
            serial_print("Created /docs/test.txt\n");
        }
    }
}