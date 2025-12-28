; struc VesaInfoBlockBuffer
;     .Signature         resb 4       ; 'VESA' signature
;     .Version           resw 1       ; VBE version
;     .OEMStringPtr      resd 1       ; Pointer to OEM string
;     .Capabilities     resb 4       ; Capabilities of graphics controller

;     .VideoModesOffset     resw 1       ; 
;     .VideoModesSegment    resw 1       ; Segment of Video Modes list

;     .CountOf64KBlocks resw 1       ; Number of 64KB memory blocks
;     .OEMSoftwareRevision resw 1       ; OEM Software revision
;     .OEMVendorNamePtr resd 1       ; Pointer to OEM Vendor Name String
;     .OEMProductNamePtr resd 1       ; Pointer to OEM Product Name String
;     .OEMProductRevPtr  resd 1       ; Pointer to OEM Product Revision String
;     .Reserved          resb 222     ; Reserved for VBE implementation scratch area
;     .OEMData           resb 256     ; Data area for OEM strings
; endstruc