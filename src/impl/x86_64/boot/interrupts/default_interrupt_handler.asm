; default_handler_x86_64.asm
; A safe default interrupt handler for all interrupts in x86-64

[BITS 64]
[GLOBAL default_interrupt_handler]
[EXTERN c_default_handler]  ; C function to handle interrupt details

default_interrupt_handler:
    ; Save registers that might be used/preserved by the handler
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; The interrupt number is pushed by the CPU or pushed by the interrupt stub.
    ; Assuming the interrupt number is on the stack at a known offset.
    ; In 64-bit mode, the stack layout depends on how the interrupt is set up.
    ; For example, if the interrupt number is pushed by the stub before calling this handler,
    ; it will be at [rsp + offset].
    ; Adjust this offset according to your interrupt stub.

    ; For demonstration, let's assume the interrupt number is at [rsp + 72]
    ; (9 registers pushed * 8 bytes each = 72 bytes)
    mov rdi, [rsp + 72]      ; Move interrupt number into RDI (first argument)

    ; Call the C handler
    call c_default_handler

    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq                    ; Return from interrupt (64-bit version)