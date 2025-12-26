// #include <stdint.h>
// struct ModeInfoBlock
// {
//     uint16_t ModeAttributes;           /* Mode attributes */
//     unsigned char WinAAttributes;      /* Window A attributes */
//     unsigned char WinBAttributes;      /* Window B attributes */
//     uint16_t WinGranularity;           /* Window granularity in k */
//     uint16_t WinSize;                  /* Window size in k */
//     uint16_t WinASegment;              /* Window A segment */
//     uint16_t WinBSegment;              /* Window B segment */
//     uint32_t WinFunctionPointer;       /* Pointer to window function */
//     uint16_t BytesPerScanLine;         /* Bytes per scanline */
//     uint16_t XResolution;              /* Horizontal resolution */
//     uint16_t YResolution;              /* Vertical resolution */
//     unsigned char XCharSize;           /* Character cell width */
//     unsigned char YCharSize;           /* Character cell height */
//     unsigned char NumberOfPlanes;      /* Number of memory planes */
//     unsigned char BitsPerPixel;        /* Bits per pixel */
//     unsigned char NumberOfBanks;       /* Number of CGA style banks */
//     unsigned char MemoryModel;         /* Memory model type */
//     unsigned char BankSize;            /* Size of CGA style banks */
//     unsigned char NumberOfImagePages;  /* Number of images pages */
//     unsigned char res1;                /* Reserved */
//     unsigned char RedMaskSize;         /* Size of direct color red mask */
//     unsigned char RedFieldPosition;    /* Bit posn of lsb of red mask */
//     unsigned char GreenMaskSize;       /* Size of direct color green mask */
//     unsigned char GreenFieldPosition;  /* Bit posn of lsb of green mask */
//     unsigned char BlueMaskSize;        /* Size of direct color blue mask */
//     unsigned char BlueFieldPosition;   /* Bit posn of lsb of blue mask */
//     unsigned char RsvdMaskSize;        /* Size of direct color res mask */   
//     unsigned char RsvdFieldPosition;   /* Bit posn of lsb of res mask */
//     unsigned char DirectColorModeInfo; /* Direct color mode attributes */
//     unsigned char res2[216];           /* Pad to 256 byte block size */

//     // Application Programming Considerations
//     //     VBE CORE FUNCTIONS VERSION 2.0 Page 77 DOCUMENT REVISION 1.1
// } __attribute__((packed));

// ModeInfoBlock *mib = (ModeInfoBlock *)0x00095000; // Placeholder address

// void init_graphics();
// void put_pixel(uint32_t x, uint32_t y, uint32_t color);