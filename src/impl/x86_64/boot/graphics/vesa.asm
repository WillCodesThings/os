; ; vesa.asm - VESA BIOS Extensions interface functions
; ; These functions handle the low-level BIOS calls for VESA

; global vbe_get_controller_info
; global vbe_get_mode_info
; global vbe_set_mode

; section .data
; vbe_mode_info_addr:    equ 0x00090000  ; Address for mode info structure
; vbe_info_block_addr:   equ 0x00095000  ; Address for VBE info block

; section .text
; bits 32

; struc VesaModeInfoBlock				;	VesaModeInfoBlock_size = 256 bytes
; 	.ModeAttributes		resw 1
; 	.FirstWindowAttributes	resb 1
; 	.SecondWindowAttributes	resb 1
; 	.WindowGranularity	resw 1		;	in KB
; 	.WindowSize		resw 1		;	in KB
; 	.FirstWindowSegment	resw 1		;	0 if not supported
; 	.SecondWindowSegment	resw 1		;	0 if not supported
; 	.WindowFunctionPtr	resd 1
; 	.BytesPerScanLine	resw 1

; 	;	Added in Revision 1.2
; 	.Width			resw 1		;	in pixels(graphics)/columns(text)
; 	.Height			resw 1		;	in pixels(graphics)/columns(text)
; 	.CharWidth		resb 1		;	in pixels
; 	.CharHeight		resb 1		;	in pixels
; 	.PlanesCount		resb 1
; 	.BitsPerPixel		resb 1
; 	.BanksCount		resb 1
; 	.MemoryModel		resb 1		;	http://www.ctyme.com/intr/rb-0274.htm#Table82
; 	.BankSize		resb 1		;	in KB
; 	.ImagePagesCount	resb 1		;	count - 1
; 	.Reserved1		resb 1		;	equals 0 in Revision 1.0-2.0, 1 in 3.0

; 	.RedMaskSize		resb 1
; 	.RedFieldPosition	resb 1
; 	.GreenMaskSize		resb 1
; 	.GreenFieldPosition	resb 1
; 	.BlueMaskSize		resb 1
; 	.BlueFieldPosition	resb 1
; 	.ReservedMaskSize	resb 1
; 	.ReservedMaskPosition	resb 1
; 	.DirectColorModeInfo	resb 1

; 	;	Added in Revision 2.0
; 	.LFBAddress		resd 1
; 	.OffscreenMemoryOffset	resd 1
; 	.OffscreenMemorySize	resw 1		;	in KB
; 	.Reserved2		resb 206	;	available in Revision 3.0, but useless for now
; endstruc

; Main:
;     ; run after getting vesa info block
;     push word [VesaInfoBlockBuffer]

; ; Function: vbe_get_controller_info
; ; Gets the VBE controller information
; ; Output: Fills the vbe_info_block structure at 0x00095000
; vbe_get_controller_info:
;     ; We need to switch to real mode to call BIOS
;     ; Save all registers
;     pusha
    
;     ; Set up the VBE Info Block signature to "VBE2" to request VBE 2.0+ info
;     mov eax, 'VBE2'
;     mov [vbe_info_block_addr], eax
    
;     ; Set up for INT 0x10, AX = 0x4F00 (Get VBE Controller Info)
;     mov ax, 0x4F00
;     mov di, vbe_info_block_addr
    
;     ; Call BIOS via interrupt
;     int 0x10
    
;     ; Check if call was successful (AX = 0x004F on success)
;     cmp ax, 0x004F
;     jne .error
    
;     ; Restore registers and return
;     popa
;     ret
    
; .error:
;     ; Handle error if needed
;     popa
;     ret

; ; Function: vbe_get_mode_info
; ; Gets information about a specific VBE mode
; ; Input: [esp+4] = Mode number
; ; Output: Fills the vbe_mode_info structure at 0x00090000
; vbe_get_mode_info:
    
;     pusha
    
;     ; Get mode number from stack
;     mov cx, [esp+36]  ; 32 for pusha + 4 for return address
    
;     ; Set up for INT 0x10, AX = 0x4F01 (Get VBE Mode Info)
;     mov ax, 0x4F01
;     mov di, vbe_mode_info_addr
    
;     ; Call BIOS via interrupt
;     int 0x10
    
;     ; Check if call was successful
;     cmp ax, 0x004F
;     jne .error
    
;     ; Restore registers and return
;     popa
;     ret
    
; .error:
;     ; Handle error if needed
;     popa
;     ret

; ; Function: vbe_set_mode
; ; Sets the video mode
; ; Input: [esp+4] = Mode number (with bit 14 set for LFB)
; ; Output: EAX = 0 on success, non-zero on error
; vbe_set_mode:
;     ; Save registers
;     pusha
    
;     ; Get mode number from stack
;     mov bx, [esp+36]  ; 32 for pusha + 4 for return address
    
;     ; Set up for INT 0x10, AX = 0x4F02 (Set VBE Mode)
;     mov ax, 0x4F02
    
;     ; Call BIOS via interrupt
;     int 0x10
    
;     ; Check if call was successful
;     cmp ax, 0x004F
;     jne .error
    
;     ; Success
;     mov dword [esp+28], 0  ; Set return value in EAX to 0 (success)
;     popa
;     ret
    
; .error:
;     ; Return error code
;     mov dword [esp+28], 1  ; Set return value in EAX to 1 (error)
;     popa
;     ret