[BITS 64]
[GLOBAL ata_primary_interrupt_handler]
[GLOBAL ata_secondary_interrupt_handler]
[EXTERN ata_primary_handle_interrupt]
[EXTERN ata_secondary_handle_interrupt]

; Primary ATA interrupt handler (IRQ 14)
ata_primary_interrupt_handler:
    ; Save all general-purpose registers
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

    ; Call the C-level handler for primary bus
    call ata_primary_handle_interrupt

    ; Send End of Interrupt (EOI) to master PIC
    mov al, 0x20
    out 0x20, al

    ; Restore all general-purpose registers
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
ata_secondary_interrupt_handler:
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

    call ata_secondary_handle_interrupt

    ; EOI: first follower, then master
    mov al, 0x20
    out 0xA0, al   ; follower PIC
    mov al, 0x20
    out 0x20, al   ; Master PIC

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

    iretq
