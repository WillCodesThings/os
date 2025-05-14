; idt_load.asm
global idt_load
extern idt_reg

idt_load:
    lidt [idt_reg]
    ret
