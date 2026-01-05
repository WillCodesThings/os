#include <memory/heap.h>
#include <shell/shell.h>
#include <stdint.h>
#include <shell/print.h>

// Simple block-based allocator with headers

// Symbol exported by linker script - marks end of kernel
extern char _kernel_end;

// Heap starts after kernel, aligned to 4KB page boundary
#define HEAP_START (((uintptr_t)&_kernel_end + 0xFFF) & ~0xFFF)
#define HEAP_SIZE 0x2000000 // 32MB heap
#define HEAP_MAGIC 0xDEADBEEF

typedef struct heap_block
{
    uint32_t magic;          // Magic number for validation
    size_t size;             // Size of this block (excluding header)
    uint8_t used;            // 1 if allocated, 0 if free
    struct heap_block *next; // Next block in list
    struct heap_block *prev; // Previous block in list
} heap_block_t;

static heap_block_t *heap_start = NULL;
static heap_block_t *heap_end = NULL;
static uint32_t total_size = 0;
static uint32_t used_size = 0;

// Initialize the heap
void heap_init(void)
{
    serial_print("Initializing kernel heap...\n");

    heap_start = (heap_block_t *)HEAP_START;
    heap_start->magic = HEAP_MAGIC;
    heap_start->size = HEAP_SIZE - sizeof(heap_block_t);
    heap_start->used = 0;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    heap_end = heap_start;
    total_size = HEAP_SIZE;
    used_size = 0;

    serial_print("Heap initialized at ");
    serial_print_hex((uint64_t)HEAP_START);
    serial_print(" (");
    serial_print_dec(HEAP_SIZE / 1024);
    serial_print(" KB)\n");
}

// Find a free block that fits the requested size
static heap_block_t *find_free_block(size_t size)
{
    heap_block_t *current = heap_start;

    while (current)
    {
        if (!current->used && current->size >= size)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// Split a block if it's too large
static void split_block(heap_block_t *block, size_t size)
{
    // Only split if there's enough space for a new block header + some data
    if (block->size >= size + sizeof(heap_block_t) + 16)
    {
        heap_block_t *new_block = (heap_block_t *)((uint8_t *)block + sizeof(heap_block_t) + size);

        new_block->magic = HEAP_MAGIC;
        new_block->size = block->size - size - sizeof(heap_block_t);
        new_block->used = 0;
        new_block->next = block->next;
        new_block->prev = block;

        if (block->next)
        {
            block->next->prev = new_block;
        }

        block->next = new_block;
        block->size = size;

        if (heap_end == block)
        {
            heap_end = new_block;
        }
    }
}

// Merge adjacent free blocks
static void merge_blocks(heap_block_t *block)
{
    // Merge with next block if it's free
    if (block->next && !block->next->used)
    {
        heap_block_t *next = block->next;

        block->size += sizeof(heap_block_t) + next->size;
        block->next = next->next;

        if (next->next)
        {
            next->next->prev = block;
        }

        if (heap_end == next)
        {
            heap_end = block;
        }
    }

    // Merge with previous block if it's free
    if (block->prev && !block->prev->used)
    {
        heap_block_t *prev = block->prev;

        prev->size += sizeof(heap_block_t) + block->size;
        prev->next = block->next;

        if (block->next)
        {
            block->next->prev = prev;
        }

        if (heap_end == block)
        {
            heap_end = prev;
        }
    }
}

// Allocate memory
void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;
    if (!heap_start)
        heap_init();

    // Align size to 8 bytes
    size = (size + 7) & ~7;

    // Find a free block
    heap_block_t *block = find_free_block(size);

    if (!block)
    {
        serial_print("kmalloc: Out of memory!\n");
        return NULL;
    }

    // Split the block if it's too large
    split_block(block, size);

    // Mark as used
    block->used = 1;
    used_size += sizeof(heap_block_t) + block->size;

    // Return pointer to data (after header)
    return (void *)((uint8_t *)block + sizeof(heap_block_t));
}

// Allocate aligned memory
void *kmalloc_aligned(size_t size, size_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        return NULL; // Alignment must be power of 2
    }

    // Allocate extra space for alignment
    size_t total_size = size + alignment;
    void *ptr = kmalloc(total_size);

    if (!ptr)
        return NULL;

    // Calculate aligned address
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);

    // Return the aligned address
    // Note: kfree won't work correctly on this pointer, but for
    // long-lived allocations like descriptor rings this is fine
    return (void *)aligned_addr;
}

// Allocate and zero memory
void *kcalloc(size_t num, size_t size)
{
    size_t total = num * size;
    void *ptr = kmalloc(total);

    if (ptr)
    {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; i++)
        {
            p[i] = 0;
        }
    }

    return ptr;
}

// Free memory
void kfree(void *ptr)
{
    if (!ptr)
        return;

    // Get block header (before the data pointer)
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));

    // Validate magic number
    if (block->magic != HEAP_MAGIC)
    {
        serial_print("kfree: Invalid pointer or corrupted heap!\n");
        return;
    }

    if (!block->used)
    {
        serial_print("kfree: Double free detected!\n");
        return;
    }

    // Mark as free
    block->used = 0;
    used_size -= sizeof(heap_block_t) + block->size;

    // Merge with adjacent free blocks
    merge_blocks(block);
}

// Reallocate memory
void *krealloc(void *ptr, size_t size)
{
    if (!ptr)
    {
        return kmalloc(size);
    }

    if (size == 0)
    {
        kfree(ptr);
        return NULL;
    }

    // Get current block
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));

    if (block->magic != HEAP_MAGIC)
    {
        return NULL;
    }

    // If new size fits in current block, just return the same pointer
    if (block->size >= size)
    {
        return ptr;
    }

    // Allocate new block
    void *new_ptr = kmalloc(size);
    if (!new_ptr)
    {
        return NULL;
    }

    // Copy old data
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    size_t copy_size = (block->size < size) ? block->size : size;

    for (size_t i = 0; i < copy_size; i++)
    {
        dst[i] = src[i];
    }

    // Free old block
    kfree(ptr);

    return new_ptr;
}

// Get heap statistics
void heap_stats(uint32_t *total, uint32_t *used, uint32_t *free)
{
    if (total)
        *total = total_size;
    if (used)
        *used = used_size;
    if (free)
        *free = total_size - used_size;
}