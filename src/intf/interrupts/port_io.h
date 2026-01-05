#pragma once
#include <stdint.h>

uint8_t inb(uint16_t port);

// Write a byte to an I/O port
void outb(uint16_t port, uint8_t data);

// Read a word (16-bit) from an I/O port
uint16_t inw(uint16_t port);
// Write a word (16-bit) to an I/O port
void outw(uint16_t port, uint16_t data);

// Read a double word (32-bit) from an I/O port
uint32_t inl(uint16_t port);

// Write a double word (32-bit) to an I/O port
void outl(uint16_t port, uint32_t data);

// I/O delay
void io_wait(void);