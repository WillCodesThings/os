[BITS 64]

; External C handler
[EXTERN c_default_handler]

; Macro to define one interrupt stub
%macro DEFINE_IRQ_STUB 1
global irq%1
irq%1:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rdi, %1        ; pass interrupt number as first argument (RDI)
    call c_default_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq
%endmacro

; Generate all 256 stubs automatically
%assign i 0
%rep 256
DEFINE_IRQ_STUB i
%assign i i+1
%endrep
