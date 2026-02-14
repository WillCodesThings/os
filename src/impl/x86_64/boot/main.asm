global start
extern long_mode_start

section .data
; Variables to store framebuffer info
global framebuffer_address
global screen_width
global screen_height
global bits_per_pixel
global pitch
global total_physical_memory

framebuffer_address: dq 0
screen_width: dd 0
screen_height: dd 0
bits_per_pixel: dd 0
pitch: dd 0
total_physical_memory: dq 0

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

    ; Parse Multiboot2 info for framebuffer and memory map
    pop ebx
    push ebx
    call parse_multiboot2_info

    call setup_page_tables
    call enable_paging

    lgdt [gdt64.pointer]
    
    ; Pass multiboot info to kernel
    pop edi
    jmp gdt64.code_segment:long_mode_start

    hlt

; Parse Multiboot2 structure to find framebuffer and memory map info
parse_multiboot2_info:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov esi, [ebp + 8]  ; Get multiboot info address from stack

    ; First 8 bytes are total_size and reserved
    add esi, 8          ; Skip to first tag

.tag_loop:
    ; Check if we've reached the end
    mov eax, [esi]      ; Tag type
    cmp eax, 0          ; Type 0 = end tag
    je .done_tags

    cmp eax, 8          ; Type 8 = framebuffer info
    je .found_framebuffer

    cmp eax, 6          ; Type 6 = memory map
    je .found_mmap

.next_tag:
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

    mov eax, [esi + 8]
    mov [framebuffer_address], eax
    mov eax, [esi + 12]
    mov [framebuffer_address + 4], eax

    mov eax, [esi + 16]
    mov [pitch], eax

    mov eax, [esi + 20]
    mov [screen_width], eax

    mov eax, [esi + 24]
    mov [screen_height], eax

    movzx eax, byte [esi + 28]
    mov [bits_per_pixel], eax

    jmp .next_tag

.found_mmap:
    ; Memory map tag structure:
    ; +0:  u32 type (6)
    ; +4:  u32 tag_size
    ; +8:  u32 entry_size
    ; +12: u32 entry_version
    ; +16+: entries, each:
    ;   +0: u64 base_addr
    ;   +8: u64 length
    ;   +16: u32 type (1=available)
    ;   +20: u32 reserved

    mov ecx, [esi + 4]      ; tag_size
    mov ebx, [esi + 8]      ; entry_size
    lea edi, [esi + 16]     ; pointer to first entry
    add ecx, esi            ; ecx = end of tag

    ; Zero the accumulator for total physical memory
    xor eax, eax
    mov [total_physical_memory], eax
    mov [total_physical_memory + 4], eax

.mmap_loop:
    cmp edi, ecx
    jge .mmap_done

    ; Check entry type: 1 = available RAM
    mov eax, [edi + 16]
    cmp eax, 1
    jne .mmap_next

    ; Add this entry's length to total_physical_memory (64-bit add)
    mov eax, [edi + 8]      ; length low 32 bits
    add [total_physical_memory], eax
    mov eax, [edi + 12]     ; length high 32 bits
    adc [total_physical_memory + 4], eax

.mmap_next:
    add edi, ebx            ; advance by entry_size
    jmp .mmap_loop

.mmap_done:
    jmp .next_tag

.done_tags:
    ; Check if framebuffer was found (screen_width != 0)
    mov eax, [screen_width]
    cmp eax, 0
    je .no_framebuffer

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

.no_framebuffer:
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