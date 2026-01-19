; Process switching assembly routines
; x86_64 System V ABI: arguments in rdi, rsi, rdx, rcx, r8, r9
[BITS 64]

section .data
    saved_kernel_rsp: dq 0

section .text
global process_jump_to_entry
global process_restore_kernel

; void process_jump_to_entry(uint64_t entry, uint64_t stack, uint64_t *kernel_rsp_ptr)
; rdi = entry point
; rsi = stack pointer
; rdx = pointer to store kernel RSP
process_jump_to_entry:
    ; Save callee-saved registers
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save entry point in callee-saved register FIRST
    mov r12, rdi    ; entry point

    ; Save kernel stack pointer to provided location
    mov [rdx], rsp

    ; Also save to our local variable for restore
    mov [rel saved_kernel_rsp], rsp

    ; Set up new stack
    mov rsp, rsi

    ; Clear most registers for clean entry
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi    ; argc = 0
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor rbp, rbp

    ; Jump to entry point (still in r12)
    call r12

    ; If process returns normally (without syscall exit),
    ; restore kernel state
    mov rsp, [rel saved_kernel_rsp]

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret


; void process_restore_kernel(uint64_t kernel_rsp)
; rdi = kernel stack pointer to restore
process_restore_kernel:
    ; Restore kernel stack
    mov rsp, rdi

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Return to kernel (process_exec continues after the call)
    ret
