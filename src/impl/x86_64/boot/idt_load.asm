; idt_load.asm
[BITS 64]
default rel

global idt_load
extern idt_reg

idt_load:
    lea rax, [idt_reg]  ; Load address of idt_reg into RAX
    lidt [rax]          ; Load IDT from that address
    ret
