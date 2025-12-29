#ifndef JPG_H
#define JPG_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <stddef.h>

typedef unsigned char byte;
typedef unsigned int uint;

/* JPEG Marker Definitions */

/* Start of Frame markers, non-differential, Huffman coding */
#define SOF0  0xC0  /* Baseline DCT */
#define SOF1  0xC1  /* Extended sequential DCT */
#define SOF2  0xC2  /* Progressive DCT */
#define SOF3  0xC3  /* Lossless (sequential) */

/* Start of Frame markers, differential, Huffman coding */
#define SOF5  0xC5  /* Differential sequential DCT */
#define SOF6  0xC6  /* Differential progressive DCT */
#define SOF7  0xC7  /* Differential lossless (sequential) */

/* Start of Frame markers, non-differential, arithmetic coding */
#define SOF9  0xC9  /* Extended sequential DCT */
#define SOF10 0xCA  /* Progressive DCT */
#define SOF11 0xCB  /* Lossless (sequential) */

/* Start of Frame markers, differential, arithmetic coding */
#define SOF13 0xCD  /* Differential sequential DCT */
#define SOF14 0xCE  /* Differential progressive DCT */
#define SOF15 0xCF  /* Differential lossless (sequential) */

/* Define Huffman Table(s) */
#define DHT 0xC4

/* JPEG extensions */
#define JPG_MARKER 0xC8

/* Define Arithmetic Coding Conditioning(s) */
#define DAC 0xCC

/* Restart interval Markers */
#define RST0 0xD0
#define RST1 0xD1
#define RST2 0xD2
#define RST3 0xD3
#define RST4 0xD4
#define RST5 0xD5
#define RST6 0xD6
#define RST7 0xD7

/* Other Markers */
#define SOI 0xD8  /* Start of Image */
#define EOI 0xD9  /* End of Image */
#define SOS 0xDA  /* Start of Scan */
#define DQT 0xDB  /* Define Quantization Table(s) */
#define DNL 0xDC  /* Define Number of Lines */
#define DRI 0xDD  /* Define Restart Interval */
#define DHP 0xDE  /* Define Hierarchical Progression */
#define EXP 0xDF  /* Expand Reference Component(s) */

/* APPN Markers */
#define APP0  0xE0
#define APP1  0xE1
#define APP2  0xE2
#define APP3  0xE3
#define APP4  0xE4
#define APP5  0xE5
#define APP6  0xE6
#define APP7  0xE7
#define APP8  0xE8
#define APP9  0xE9
#define APP10 0xEA
#define APP11 0xEB
#define APP12 0xEC
#define APP13 0xED
#define APP14 0xEE
#define APP15 0xEF

/* Misc Markers */
#define JPG0  0xF0
#define JPG1  0xF1
#define JPG2  0xF2
#define JPG3  0xF3
#define JPG4  0xF4
#define JPG5  0xF5
#define JPG6  0xF6
#define JPG7  0xF7
#define JPG8  0xF8
#define JPG9  0xF9
#define JPG10 0xFA
#define JPG11 0xFB
#define JPG12 0xFC
#define JPG13 0xFD
#define COM   0xFE
#define TEM   0x01

/* Huffman Table Structure */
typedef struct {
    byte offsets[17];
    byte symbols[176];
    uint codes[176];
    int set;
} HuffmanTable;

/* Quantization Table Structure */
typedef struct {
    uint table[64];
    int set;
} QuantizationTable;

/* Color Component Structure */
typedef struct {
    byte horizontalSamplingFactor;
    byte verticalSamplingFactor;
    byte quantizationTableID;
    byte huffmanDCTableID;
    byte huffmanACTableID;
    int usedInFrame;
    int usedInScan;
} ColorComponent;

/* MCU (Minimum Coded Unit) Structure */
typedef struct {
    union {
        int y[64];
        int r[64];
    };
    union {
        int cb[64];
        int g[64];
    };
    union {
        int cr[64];
        int b[64];
    };
} MCU;

/* Dynamic byte array structure (replaces std::vector<byte>) */
typedef struct {
    byte* data;
    size_t size;
    size_t capacity;
} ByteArray;

/* Header Structure */
typedef struct {
    QuantizationTable quantizationTables[4];
    HuffmanTable huffmanDCTables[4];
    HuffmanTable huffmanACTables[4];
    ColorComponent colorComponents[3];

    byte frameType;
    uint height;
    uint width;
    byte numComponents;
    int zeroBased;

    byte componentsInScan;
    byte startOfSelection;
    byte endOfSelection;
    byte successiveApproximationHigh;
    byte successiveApproximationLow;

    uint restartInterval;

    MCU* blocks;

    int valid;

    uint blockHeight;
    uint blockWidth;
    uint blockHeightReal;
    uint blockWidthReal;

    byte horizontalSamplingFactor;
    byte verticalSamplingFactor;
    
    char* filename;
    ByteArray huffmanData;
} Header;

/* ZigZag Map */
static const byte zigZagMap[] = {
    0,   1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* IDCT scaling factors - computed using math functions for precision matching C++ */
#define m0 ((float)(2.0 * cos(1.0 / 16.0 * 2.0 * M_PI)))
#define m1 ((float)(2.0 * cos(2.0 / 16.0 * 2.0 * M_PI)))
#define m3 ((float)(2.0 * cos(2.0 / 16.0 * 2.0 * M_PI)))
#define m5 ((float)(2.0 * cos(3.0 / 16.0 * 2.0 * M_PI)))
#define m2 ((float)(m0 - m5))
#define m4 ((float)(m0 + m5))

#define s0 ((float)(cos(0.0 / 16.0 * M_PI) / sqrt(8)))
#define s1 ((float)(cos(1.0 / 16.0 * M_PI) / 2.0))
#define s2 ((float)(cos(2.0 / 16.0 * M_PI) / 2.0))
#define s3 ((float)(cos(3.0 / 16.0 * M_PI) / 2.0))
#define s4 ((float)(cos(4.0 / 16.0 * M_PI) / 2.0))
#define s5 ((float)(cos(5.0 / 16.0 * M_PI) / 2.0))
#define s6 ((float)(cos(6.0 / 16.0 * M_PI) / 2.0))
#define s7 ((float)(cos(7.0 / 16.0 * M_PI) / 2.0))

/* Helper functions for ByteArray */
void bytearray_init(ByteArray* arr);
void bytearray_push(ByteArray* arr, byte value);
void bytearray_free(ByteArray* arr);

#endif
