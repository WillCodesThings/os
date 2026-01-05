section .multiboot_header
align 8
header_start:
    dd 0xe85250d6                ; magic number (multiboot 2)
    dd 0                         ; architecture 0 (protected mode i386)
    dd header_end - header_start ; header length
    ; checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; framebuffer tag
align 8
framebuffer_tag_start:
    dw 5                        ; type = framebuffer
    dw 0                        ; flags
    dd framebuffer_tag_end - framebuffer_tag_start ; size
    dd 1280                     ; width
    dd 720                     ; height
    dd 32                       ; depth (bits per pixel)
framebuffer_tag_end:

    ; end tag
align 8
    dw 0                        ; type
    dw 0                        ; flags
    dd 8                        ; size
header_end: