#pragma once
#include <stdint.h>
#include <stddef.h>

uint8_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void strcpy(char *dest, const char *src);

