; e1000 network card interrupt handler

section .text
global e1000_interrupt_handler
extern e1000_handle_interrupt

e1000_interrupt_handler:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Align stack
    sub rsp, 8

    ; Call C handler
    call e1000_handle_interrupt

    ; Restore stack
    add rsp, 8

    ; Send EOI to PIC
    ; e1000 is usually on IRQ 11 (slave PIC)
    mov al, 0x20
    out 0xA0, al    ; EOI to slave PIC
    out 0x20, al    ; EOI to master PIC

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq
