#pragma once

#include <stdint.h>
#include <stddef.h>

// Set memory to a value
void *memset(void *ptr, int value, size_t num);

// Copy memory
void *memcpy(void *dest, const void *src, size_t num);

// Move memory (handles overlapping regions)
void *memmove(void *dest, const void *src, size_t num);

// Compare memory
int memcmp(const void *ptr1, const void *ptr2, size_t num);