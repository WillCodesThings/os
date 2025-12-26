global start
extern long_mode_start

section .data
; Define a structure for VBE mode info at a fixed address
vbe_mode_info: equ 0x00090000  ; Fixed address for VBE info

; External C function declarations
extern find_best_vesa_mode
extern set_vesa_mode
; External C variable declarations
extern selected_mode
extern framebuffer_address
extern screen_width
extern screen_height
extern bits_per_pixel
extern pitch

section .text
bits 32
global start:
start:
	mov esp, stack_top

	call check_multiboot
	call check_cpuid
	call check_long_mode

	; --- Setup VESA graphics mode before paging ---
    call setup_vesa_graphics

	; Now map framebuffer in higher-half kernel space
	call map_framebuffer

	call setup_page_tables
	call enable_paging

	lgdt [gdt64.pointer]
	jmp gdt64.code_segment:long_mode_start

	hlt

; --- VESA graphics setup routine ---
; setup_vesa_graphics:
; 	; Find the best available VESA mode for our desired resolution (1920x1080x32)
; 	call find_best_vesa_mode
	
; 	; Check if we found a valid mode
; 	test ax, ax
; 	jz vesa_error
	
; 	; Store the selected mode
; 	mov [selected_mode], ax
	
; 	; Set the mode
; 	push ax
; 	call set_vesa_mode
; 	add esp, 4
	
; 	; Check for errors
; 	test eax, eax
; 	jnz vesa_error
	
; 	ret

; vesa_error:
; 	mov al, "V"
; 	jmp error

; Map framebuffer in page tables
map_framebuffer:
	; Get framebuffer address
	mov eax, [framebuffer_address]
	test eax, eax
	jz .skip_mapping  ; Skip if no framebuffer

	; Calculate how many 2MB pages we need
	; Get total framebuffer size (height * pitch)
	movzx ecx, word [screen_height]
	movzx edx, word [pitch]
	mul ecx
	mov ecx, edx
	
	; Divide by 2MB (0x200000) to get number of pages 
	; and add 1 to round up
	add eax, 0x1FFFFF
	shr eax, 21
	
	; Store number of pages needed in ecx
	mov ecx, eax
	
	; Now map each page
	mov eax, [framebuffer_address]
	and eax, 0xFFE00000  ; Align to 2MB boundary
	
	; Map the framebuffer pages in the page table
	; For simplicity, we'll use 1:1 mapping for now
	mov edx, eax  ; Physical address
	or edx, 0b10000011  ; present, writable, huge page
	
	; Find a free spot in page table L2
	mov edi, page_table_l2
	add edi, 512 * 8  ; Skip first 512 entries (used by kernel)
	
.map_page:
	mov [edi], edx
	add edi, 8      ; Next page table entry
	add edx, 0x200000  ; Next physical 2MB
	loop .map_page
	
.skip_mapping:
	ret

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