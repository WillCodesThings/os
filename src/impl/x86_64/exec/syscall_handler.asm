; Syscall interrupt handler (int 0x80)
; Uses Linux-like calling convention:
; Syscall number in RAX
; Arguments in RDI, RSI, RDX, R10, R8, R9
; Return value in RAX

section .text
global syscall_interrupt_handler
extern syscall_handler

syscall_interrupt_handler:
    ; Save all general-purpose registers
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; Align stack to 16 bytes
    sub rsp, 8

    ; Set up arguments for syscall_handler
    ; syscall_handler(num, arg1, arg2, arg3, arg4, arg5)
    ; C calling convention: rdi, rsi, rdx, rcx, r8, r9

    ; RAX was pushed last (syscall number)
    ; Get it from stack: rsp+8 is rax
    mov rdi, [rsp + 8]       ; num = rax (syscall number)

    ; Arguments came in rdi, rsi, rdx, r10, r8, r9
    ; We pushed them, so:
    ; rdi at [rsp + 8 + 7*8] = [rsp + 64]
    ; rsi at [rsp + 8 + 6*8] = [rsp + 56]
    ; rdx at [rsp + 8 + 5*8] = [rsp + 48]
    ; r10 at [rsp + 8 + 2*8] = [rsp + 24]  (r10 is arg4, not rcx in syscalls)
    ; r8  at [rsp + 8 + 3*8] = [rsp + 32]
    ; r9  at [rsp + 8 + 1*8] = [rsp + 16]

    mov rsi, [rsp + 64]      ; arg1 = original rdi
    mov rdx, [rsp + 56]      ; arg2 = original rsi
    mov rcx, [rsp + 48]      ; arg3 = original rdx
    mov r8,  [rsp + 24]      ; arg4 = original r10
    mov r9,  [rsp + 32]      ; arg5 = original r8

    ; Call the C handler
    call syscall_handler

    ; Return value is in RAX
    ; Store it back to saved RAX position
    mov [rsp + 8], rax

    ; Restore stack alignment
    add rsp, 8

    ; Restore registers (RAX will have return value)
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    iretq
