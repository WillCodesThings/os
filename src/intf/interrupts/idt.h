// idt.h

#include <stdint.h>

struct idt_entry
{
    uint16_t offset_low;  // bits 0..15
    uint16_t selector;    // segment selector
    uint8_t ist;          // bits 0..2 = IST, rest zero
    uint8_t type_attr;    // type and attributes
    uint16_t offset_mid;  // bits 16..31
    uint32_t offset_high; // bits 32..63
    uint32_t zero;        // reserved
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256
extern struct idt_entry idt[IDT_ENTRIES];

void idt_set_gate(int n, uint64_t handler);
void idt_install();
void idt_load();
