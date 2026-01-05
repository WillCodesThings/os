[BITS 64]
[GLOBAL mouse_interrupt_handler]
[EXTERN mouse_handle_interrupt]

mouse_interrupt_handler:
    ; Save general-purpose registers (except rsp)
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    

    ; Save segment registers (optional but good practice)
    ; Not strictly necessary in long mode, but may help prevent bugs
    ; push ds
    ; push es
    ; push fs
    ; push gs

    ; Call the C-level handler
    call mouse_handle_interrupt

    ; Send End of Interrupt (EOI) to the PIC
    mov al, 0x20
    out 0xA0, al ; follower PIC
    out 0x20, al ; master PIC


    ; Restore segment registers (if saved)
    ; pop gs
    ; pop fs
    ; pop es
    ; pop ds

    ; Restore general-purpose registers
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Return from interrupt
    iretq
