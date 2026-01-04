// idt.c

#include <interrupts/idt.h>

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr idt_reg;

void idt_set_gate(int n, uint64_t handler)
{
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = 0x08; // Kernel code segment
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E; // Interrupt gate, present, ring 0
    idt[n].offset_mid = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void idt_install(void)
{
    // Zero the IDT memory directly
    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        idt[i] = (struct idt_entry){0};
    }

    idt_reg.limit = sizeof(struct idt_entry) * IDT_ENTRIES - 1;
    idt_reg.base = (uint64_t)&idt;

    idt_load();
}
