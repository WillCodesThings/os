; idt_load.asm
[BITS 64]

global idt_load
extern idt_reg

idt_load:
    lidt [rel idt_reg]  ; Use RIP-relative addressing for 64-bit
    ret
