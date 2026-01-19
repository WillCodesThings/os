global start
extern long_mode_start

section .data
; Variables to store framebuffer info
global framebuffer_address
global screen_width
global screen_height
global bits_per_pixel
global pitch

framebuffer_address: dq 0
screen_width: dd 0
screen_height: dd 0
bits_per_pixel: dd 0
pitch: dd 0

section .text
bits 32
start:
    cli                 ; Disable interrupts immediately
    mov esp, stack_top
    
    ; EBX contains the physical address of the Multiboot2 info structure
    push ebx  ; Save it

    call check_multiboot
    call check_cpuid
    call check_long_mode

    ; Parse Multiboot2 info for framebuffer
    pop ebx
    push ebx
    call parse_multiboot2_framebuffer

    call setup_page_tables
    call enable_paging

    lgdt [gdt64.pointer]
    
    ; Pass multiboot info to kernel
    pop edi
    jmp gdt64.code_segment:long_mode_start

    hlt

; Parse Multiboot2 structure to find framebuffer info
parse_multiboot2_framebuffer:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    
    mov esi, [ebp + 8]  ; Get multiboot info address from stack
    
    ; First 8 bytes are total_size and reserved
    mov ecx, [esi]      ; Total size
    add esi, 8          ; Skip to first tag
    
.tag_loop:
    ; Check if we've reached the end
    mov eax, [esi]      ; Tag type
    cmp eax, 0          ; Type 0 = end tag
    je .not_found
    
    cmp eax, 8          ; Type 8 = framebuffer info
    je .found_framebuffer
    
    ; Move to next tag
    mov edx, [esi + 4]  ; Tag size
    add esi, edx        ; Skip this tag
    
    ; Align to 8-byte boundary
    add esi, 7
    and esi, ~7
    
    jmp .tag_loop

.found_framebuffer:
    ; Framebuffer tag structure:
    ; +0:  u32 type (8)
    ; +4:  u32 size
    ; +8:  u64 framebuffer_addr
    ; +16: u32 framebuffer_pitch
    ; +20: u32 framebuffer_width
    ; +24: u32 framebuffer_height
    ; +28: u8  framebuffer_bpp
    ; +29: u8  framebuffer_type (1 = RGB)
    
    ; Read framebuffer address (64-bit, but we only need lower 32 bits)
    mov eax, [esi + 8]
    mov [framebuffer_address], eax
    mov eax, [esi + 12]
    mov [framebuffer_address + 4], eax
    
    ; Read other info
    mov eax, [esi + 16]
    mov [pitch], eax
    
    mov eax, [esi + 20]
    mov [screen_width], eax
    
    mov eax, [esi + 24]
    mov [screen_height], eax
    
    movzx eax, byte [esi + 28]
    mov [bits_per_pixel], eax
    
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.not_found:
    ; Framebuffer not found - error
    mov al, 'F'
    mov [0xb8000], al
    cli
    hlt

check_multiboot:
    cmp eax, 0x36d76289
    jne .no_multiboot
    ret
.no_multiboot:
    mov al, 'M'
    jmp error

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, 'C'
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, 'L'
    jmp error

setup_page_tables:
    ; Map P4 table
    mov eax, p3_table
    or eax, 0b11  ; present + writable
    mov [p4_table], eax
    
    ; Map multiple P3 entries to cover first 4GB
    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax          ; 0-1GB
    
    mov eax, p2_table_2
    or eax, 0b11
    mov [p3_table + 8], eax      ; 1-2GB
    
    mov eax, p2_table_3
    or eax, 0b11
    mov [p3_table + 16], eax     ; 2-3GB
    
    mov eax, p2_table_4
    or eax, 0b11
    mov [p3_table + 24], eax     ; 3-4GB
    
    ; Identity map first 1GB (0x00000000 - 0x3FFFFFFF)
    xor ecx, ecx
.map_p2_1:
    mov eax, ecx
    shl eax, 21              ; ecx * 2MB (shift left 21 bits)
    or eax, 0b10000011       ; present + writable + huge page
    mov [p2_table + ecx * 8], eax
    mov dword [p2_table + ecx * 8 + 4], 0  ; upper 32 bits
    inc ecx
    cmp ecx, 512
    jne .map_p2_1
    
    ; Identity map second 1GB (0x40000000 - 0x7FFFFFFF)
    xor ecx, ecx
.map_p2_2:
    mov eax, ecx
    shl eax, 21
    add eax, 0x40000000
    or eax, 0b10000011
    mov [p2_table_2 + ecx * 8], eax
    mov dword [p2_table_2 + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map_p2_2
    
    ; Identity map third 1GB (0x80000000 - 0xBFFFFFFF)
    xor ecx, ecx
.map_p2_3:
    mov eax, ecx
    shl eax, 21
    add eax, 0x80000000
    or eax, 0b10000011
    mov [p2_table_3 + ecx * 8], eax
    mov dword [p2_table_3 + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map_p2_3
    
    ; Identity map fourth 1GB (0xC0000000 - 0xFFFFFFFF)
    xor ecx, ecx
.map_p2_4:
    mov eax, ecx
    shl eax, 21
    add eax, 0xC0000000
    or eax, 0b10000011
    mov [p2_table_4 + ecx * 8], eax
    mov dword [p2_table_4 + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map_p2_4
    
    ret

enable_paging:
    ; Load P4 to cr3
    mov eax, p4_table
    mov cr3, eax
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ret

error:
    mov dword [0xb8000], 0x4f524f45
    mov [0xb8004], al
    hlt

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
p2_table_2:
    resb 4096
p2_table_3:
    resb 4096
p2_table_4:
    resb 4096
stack_bottom:
    resb 4096 * 4
stack_top:

section .rodata
gdt64:
    dq 0
.code_segment: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64