global start
extern long_mode_start

section .data
; Define a structure for VBE mode info at a fixed address
vbe_mode_info: equ 0x00090000  ; Fixed address for VBE info

; Variables to store graphics mode information
global framebuffer_address, screen_width, screen_height, bits_per_pixel, pitch
framebuffer_address: dd 0      ; Linear framebuffer physical address
screen_width:       dw 0       ; Screen width in pixels
screen_height:      dw 0       ; Screen height in pixels  
bits_per_pixel:     db 0       ; Bits per pixel
pitch:              dw 0       ; Bytes per scanline

section .text
bits 32
global start:
start:
	mov esp, stack_top

	call check_multiboot
	call check_cpuid
	call check_long_mode

	; --- Setup VESA graphics mode before paging ---
    ; call setup_vesa_graphics

	call setup_page_tables
	call enable_paging

	lgdt [gdt64.pointer]
	jmp gdt64.code_segment:long_mode_start

	hlt

; --- VESA graphics setup routine ---
; setup_vesa_graphics:
;     ; Set VESA mode 0x118 (1024x768x32bpp) with linear framebuffer
;     mov ax, 0x4F02
;     mov bx, 0x118 | 0x4000    ; Mode 0x118 + linear framebuffer flag
;     int 0x10
;     cmp ax, 0x004F
;     jne vesa_error

;     ; Get VESA mode info
;     ; Set ES:DI to point to our VBE info structure buffer
;     mov di, vbe_mode_info     ; Destination address for mode info
;     mov ax, 0x4F01            ; VBE get mode info
;     mov cx, 0x118             ; Mode we want info about
;     int 0x10
;     cmp ax, 0x004F
;     jne vesa_error

;     ; Store relevant info in our variables
;     ; Note: We access the mode info structure directly
;     ; framebuffer at offset 0x28
;     mov eax, [vbe_mode_info + 0x28]
;     mov [framebuffer_address], eax

;     ; screen width at offset 0x12
;     mov ax, [vbe_mode_info + 0x12]
;     mov [screen_width], ax

;     ; screen height at offset 0x14
;     mov ax, [vbe_mode_info + 0x14]
;     mov [screen_height], ax

;     ; bits per pixel at offset 0x19
;     mov al, [vbe_mode_info + 0x19]
;     mov [bits_per_pixel], al

;     ; pitch (bytes per scanline) at offset 0x10
;     mov ax, [vbe_mode_info + 0x10]
;     mov [pitch], ax

;     ret

; vesa_error:
;     mov al, "V"
;     jmp error

check_multiboot:
	cmp eax, 0x36d76289
	jne .no_multiboot
	ret
.no_multiboot:
	mov al, "M"
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
	mov al, "C"
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
	mov al, "L"
	jmp error

setup_page_tables:
	mov eax, page_table_l3
	or eax, 0b11 ; present, writable
	mov [page_table_l4], eax
	
	mov eax, page_table_l2
	or eax, 0b11 ; present, writable
	mov [page_table_l3], eax

	mov ecx, 0 ; counter
.loop:

	mov eax, 0x200000 ; 2MiB
	mul ecx
	or eax, 0b10000011 ; present, writable, huge page
	mov [page_table_l2 + ecx * 8], eax

	inc ecx ; increment counter
	cmp ecx, 512 ; checks if the whole table is mapped
	jne .loop ; if not, continue

	ret

enable_paging:
	; pass page table location to cpu
	mov eax, page_table_l4
	mov cr3, eax

	; enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging
	mov eax, cr0
	or eax, 1 << 31
	mov cr0, eax

	ret

error:
	; print "ERR: X" where X is the error code
	mov dword [0xb8000], 0x4f524f45
	mov dword [0xb8004], 0x4f3a4f52
	mov dword [0xb8008], 0x4f204f20
	mov byte  [0xb800a], al
	hlt

section .bss
align 4096
page_table_l4:
	resb 4096
page_table_l3:
	resb 4096
page_table_l2:
	resb 4096
stack_bottom:
	resb 4096 * 4
stack_top:

section .rodata
gdt64:
	dq 0 ; zero entry
.code_segment: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; code segment
.pointer:
	dw $ - gdt64 - 1 ; length
	dq gdt64 ; address