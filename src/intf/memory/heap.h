#pragma once
#include <stdint.h>
#include <stddef.h>

void heap_init(void);

void *kmalloc(size_t size);

void *kmalloc_aligned(size_t size, size_t alignment);

void *kcalloc(size_t num, size_t size);

void kfree(void *ptr);

void *krealloc(void *ptr, size_t new_size);

void heap_stats(uint32_t *total_size, uint32_t *used_size, uint32_t *free_size);

void heap_dump(void);