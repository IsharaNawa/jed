#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "jpg_c.h"

/* ByteArray implementation */
void bytearray_init(ByteArray* arr) {
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

void bytearray_push(ByteArray* arr, byte value) {
    if (arr->size >= arr->capacity) {
        size_t newCapacity = arr->capacity == 0 ? 16 : arr->capacity * 2;
        byte* newData = (byte*)realloc(arr->data, newCapacity);
        if (newData == NULL) {
            printf("Error - Memory allocation failed\n");
            return;
        }
        arr->data = newData;
        arr->capacity = newCapacity;
    }
    arr->data[arr->size++] = value;
}

void bytearray_free(ByteArray* arr) {
    if (arr->data != NULL) {
        free(arr->data);
        arr->data = NULL;
    }
    arr->size = 0;
    arr->capacity = 0;
}

/* BMPImage structure for encoder */
typedef struct {
    uint height;
    uint width;
    MCU* blocks;
    uint blockHeight;
    uint blockWidth;
} BMPImage;

/* Encoder context - uses Header from jpg_c.h for compatibility */

/* Quantization tables for quality 100 */
static const QuantizationTable qTableY100 = {
    {
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1
    },
    1
};

static const QuantizationTable qTableCbCr100 = {
    {
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1
    },
    1
};

static const QuantizationTable* qTables100[] = { &qTableY100, &qTableCbCr100, &qTableCbCr100 };

/* Huffman tables */
static HuffmanTable hDCTableY = {
    { 0, 0, 1, 6, 7, 8, 9, 10, 11, 12, 12, 12, 12, 12, 12, 12, 12 },
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b },
    { 0 },
    0
};

static HuffmanTable hDCTableCbCr = {
    { 0, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 12, 12, 12, 12 },
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b },
    { 0 },
    0
};

static HuffmanTable hACTableY = {
    { 0, 0, 2, 3, 6, 9, 11, 15, 18, 23, 28, 32, 36, 36, 36, 37, 162 },
    {
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
        0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
        0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa
    },
    { 0 },
    0
};

static HuffmanTable hACTableCbCr = {
    { 0, 0, 2, 3, 5, 9, 13, 16, 20, 27, 32, 36, 40, 40, 41, 43, 162 },
    {
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
        0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
        0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa
    },
    { 0 },
    0
};

static HuffmanTable* dcTables[] = { &hDCTableY, &hDCTableCbCr, &hDCTableCbCr };
static HuffmanTable* acTables[] = { &hACTableY, &hACTableCbCr, &hACTableCbCr };

/* Helper to get MCU component by index */
static int* getMCUComponent(MCU* mcu, uint index) {
    switch (index) {
        case 0: return mcu->y;
        case 1: return mcu->cb;
        case 2: return mcu->cr;
        default: return NULL;
    }
}

/* ============================================== */
/*          Stage 1: Read BMP                     */
/* ============================================== */

/* Helper function to read a 4-byte integer in little-endian */
static uint getInt(FILE* inFile) {
    int b0 = fgetc(inFile);
    int b1 = fgetc(inFile);
    int b2 = fgetc(inFile);
    int b3 = fgetc(inFile);
    return (b0 << 0) + (b1 << 8) + (b2 << 16) + (b3 << 24);
}

/* Helper function to read a 2-byte short integer in little-endian */
static uint getShort(FILE* inFile) {
    int b0 = fgetc(inFile);
    int b1 = fgetc(inFile);
    return (b0 << 0) + (b1 << 8);
}

static BMPImage readBMP(const char* filename) {
    BMPImage image = { 0, 0, NULL, 0, 0 };

    /* open file */
    printf("Reading %s...\n", filename);
    FILE* inFile = fopen(filename, "rb");
    if (inFile == NULL) {
        printf("Error - Error opening input file\n");
        return image;
    }

    if (fgetc(inFile) != 'B' || fgetc(inFile) != 'M') {
        printf("Error - Invalid BMP file\n");
        fclose(inFile);
        return image;
    }

    getInt(inFile); /* size */
    getInt(inFile); /* nothing */
    if (getInt(inFile) != 0x1A) {
        printf("Error - Invalid offset\n");
        fclose(inFile);
        return image;
    }
    if (getInt(inFile) != 12) {
        printf("Error - Invalid DIB size\n");
        fclose(inFile);
        return image;
    }
    image.width = getShort(inFile);
    image.height = getShort(inFile);
    if (getShort(inFile) != 1) {
        printf("Error - Invalid number of planes\n");
        fclose(inFile);
        return image;
    }
    if (getShort(inFile) != 24) {
        printf("Error - Invalid bit depth\n");
        fclose(inFile);
        return image;
    }

    if (image.height == 0 || image.width == 0) {
        printf("Error - Invalid dimensions\n");
        fclose(inFile);
        return image;
    }

    image.blockHeight = (image.height + 7) / 8;
    image.blockWidth = (image.width + 7) / 8;

    image.blocks = (MCU*)calloc(image.blockHeight * image.blockWidth, sizeof(MCU));
    if (image.blocks == NULL) {
        printf("Error - Memory error\n");
        fclose(inFile);
        return image;
    }

    const uint paddingSize = image.width % 4;

    for (uint y = image.height - 1; y < image.height; --y) {
        const uint blockRow = y / 8;
        const uint pixelRow = y % 8;
        for (uint x = 0; x < image.width; ++x) {
            const uint blockColumn = x / 8;
            const uint pixelColumn = x % 8;
            const uint blockIndex = blockRow * image.blockWidth + blockColumn;
            const uint pixelIndex = pixelRow * 8 + pixelColumn;
            image.blocks[blockIndex].b[pixelIndex] = fgetc(inFile);
            image.blocks[blockIndex].g[pixelIndex] = fgetc(inFile);
            image.blocks[blockIndex].r[pixelIndex] = fgetc(inFile);
        }
        for (uint i = 0; i < paddingSize; ++i) {
            fgetc(inFile);
        }
    }

    fclose(inFile);
    return image;
}

/* ============================================== */
/*       Stage 2: Color Conversion               */
/* ============================================== */

/* Convert all pixels in a block from RGB color space to YCbCr */
static void RGBToYCbCrBlock(MCU* block) {
    for (uint py = 0; py < 8; ++py) {
        for (uint px = 0; px < 8; ++px) {
            const uint pixel = py * 8 + px;
            int yVal  =  0.2990 * block->r[pixel] + 0.5870 * block->g[pixel] + 0.1140 * block->b[pixel] - 128;
            int cbVal = -0.1687 * block->r[pixel] - 0.3313 * block->g[pixel] + 0.5000 * block->b[pixel];
            int crVal =  0.5000 * block->r[pixel] - 0.4187 * block->g[pixel] - 0.0813 * block->b[pixel];
            if (yVal  < -128) yVal  = -128;
            if (yVal  >  127) yVal  =  127;
            if (cbVal < -128) cbVal = -128;
            if (cbVal >  127) cbVal =  127;
            if (crVal < -128) crVal = -128;
            if (crVal >  127) crVal =  127;
            block->y[pixel]  = yVal;
            block->cb[pixel] = cbVal;
            block->cr[pixel] = crVal;
        }
    }
}

/* Convert all pixels from RGB color space to YCbCr */
static void RGBToYCbCr(const BMPImage* image) {
    for (uint y = 0; y < image->blockHeight; ++y) {
        for (uint x = 0; x < image->blockWidth; ++x) {
            RGBToYCbCrBlock(&image->blocks[y * image->blockWidth + x]);
        }
    }
}

/* ============================================== */
/*       Stage 2.5: Downsampling                  */
/* ============================================== */

/* Downsample chrominance components using nearest-neighbor */
static void downsample(BMPImage* image, Header* ctx) {
    const uint vSamp = ctx->verticalSamplingFactor;
    const uint hSamp = ctx->horizontalSamplingFactor;

    printf("Downsampling Cb/Cr with factors %ux%u...\n", hSamp, vSamp);

    /* Calculate real block dimensions (padded to sampling factor multiples) */
    ctx->blockHeightReal = (image->blockHeight + vSamp - 1) / vSamp * vSamp;
    ctx->blockWidthReal = (image->blockWidth + hSamp - 1) / hSamp * hSamp;

    /* If we need more blocks, reallocate */
    if (ctx->blockHeightReal != image->blockHeight || ctx->blockWidthReal != image->blockWidth) {
        MCU* newBlocks = (MCU*)calloc(ctx->blockHeightReal * ctx->blockWidthReal, sizeof(MCU));
        if (newBlocks == NULL) {
            printf("Error - Memory error during downsampling\n");
            return;
        }
        /* Copy existing blocks */
        for (uint y = 0; y < image->blockHeight; ++y) {
            for (uint x = 0; x < image->blockWidth; ++x) {
                newBlocks[y * ctx->blockWidthReal + x] = image->blocks[y * image->blockWidth + x];
            }
        }
        free(image->blocks);
        image->blocks = newBlocks;
    }

    /* Downsample: collect Cb/Cr values from multiple blocks into the top-left block of each MCU group */
    for (uint y = 0; y < ctx->blockHeightReal; y += vSamp) {
        for (uint x = 0; x < ctx->blockWidthReal; x += hSamp) {
            MCU* cbcrBlock = &image->blocks[y * ctx->blockWidthReal + x];
            
            /* For each pixel in the destination Cb/Cr block */
            for (uint py = 0; py < 8; ++py) {
                for (uint px = 0; px < 8; ++px) {
                    const uint dstPixel = py * 8 + px;
                    
                    /* Calculate which source block and pixel this comes from */
                    const uint srcBlockV = py / (8 / vSamp);
                    const uint srcBlockH = px / (8 / hSamp);
                    const uint srcPixelRow = (py % (8 / vSamp)) * vSamp;
                    const uint srcPixelCol = (px % (8 / hSamp)) * hSamp;
                    const uint srcPixel = srcPixelRow * 8 + srcPixelCol;
                    
                    /* Get source block (clamped to valid range) */
                    const uint srcY = y + srcBlockV;
                    const uint srcX = x + srcBlockH;
                    if (srcY < ctx->blockHeightReal && srcX < ctx->blockWidthReal) {
                        const MCU* srcBlock = &image->blocks[srcY * ctx->blockWidthReal + srcX];
                        cbcrBlock->cb[dstPixel] = srcBlock->cb[srcPixel];
                        cbcrBlock->cr[dstPixel] = srcBlock->cr[srcPixel];
                    }
                }
            }
        }
    }
}

/* ============================================== */
/*       Stage 3: Forward DCT                     */
/* ============================================== */

/* Perform 1-D FDCT on all columns and rows of a block component */
static void forwardDCTBlockComponent(int* component) {
    /* Column pass */
    for (uint i = 0; i < 8; ++i) {
        const float a0 = component[0 * 8 + i];
        const float a1 = component[1 * 8 + i];
        const float a2 = component[2 * 8 + i];
        const float a3 = component[3 * 8 + i];
        const float a4 = component[4 * 8 + i];
        const float a5 = component[5 * 8 + i];
        const float a6 = component[6 * 8 + i];
        const float a7 = component[7 * 8 + i];

        const float b0 = a0 + a7;
        const float b1 = a1 + a6;
        const float b2 = a2 + a5;
        const float b3 = a3 + a4;
        const float b4 = a3 - a4;
        const float b5 = a2 - a5;
        const float b6 = a1 - a6;
        const float b7 = a0 - a7;

        const float c0 = b0 + b3;
        const float c1 = b1 + b2;
        const float c2 = b1 - b2;
        const float c3 = b0 - b3;
        const float c4 = b4;
        const float c5 = b5 - b4;
        const float c6 = b6 - c5;
        const float c7 = b7 - b6;

        const float d0 = c0 + c1;
        const float d1 = c0 - c1;
        const float d2 = c2;
        const float d3 = c3 - c2;
        const float d4 = c4;
        const float d5 = c5;
        const float d6 = c6;
        const float d7 = c5 + c7;
        const float d8 = c4 - c6;

        const float e0 = d0;
        const float e1 = d1;
        const float e2 = d2 * m1;
        const float e3 = d3;
        const float e4 = d4 * m2;
        const float e5 = d5 * m3;
        const float e6 = d6 * m4;
        const float e7 = d7;
        const float e8 = d8 * m5;

        const float f0 = e0;
        const float f1 = e1;
        const float f2 = e2 + e3;
        const float f3 = e3 - e2;
        const float f4 = e4 + e8;
        const float f5 = e5 + e7;
        const float f6 = e6 + e8;
        const float f7 = e7 - e5;

        const float g0 = f0;
        const float g1 = f1;
        const float g2 = f2;
        const float g3 = f3;
        const float g4 = f4 + f7;
        const float g5 = f5 + f6;
        const float g6 = f5 - f6;
        const float g7 = f7 - f4;

        component[0 * 8 + i] = g0 * s0;
        component[4 * 8 + i] = g1 * s4;
        component[2 * 8 + i] = g2 * s2;
        component[6 * 8 + i] = g3 * s6;
        component[5 * 8 + i] = g4 * s5;
        component[1 * 8 + i] = g5 * s1;
        component[7 * 8 + i] = g6 * s7;
        component[3 * 8 + i] = g7 * s3;
    }
    
    /* Row pass */
    for (uint i = 0; i < 8; ++i) {
        const float a0 = component[i * 8 + 0];
        const float a1 = component[i * 8 + 1];
        const float a2 = component[i * 8 + 2];
        const float a3 = component[i * 8 + 3];
        const float a4 = component[i * 8 + 4];
        const float a5 = component[i * 8 + 5];
        const float a6 = component[i * 8 + 6];
        const float a7 = component[i * 8 + 7];

        const float b0 = a0 + a7;
        const float b1 = a1 + a6;
        const float b2 = a2 + a5;
        const float b3 = a3 + a4;
        const float b4 = a3 - a4;
        const float b5 = a2 - a5;
        const float b6 = a1 - a6;
        const float b7 = a0 - a7;

        const float c0 = b0 + b3;
        const float c1 = b1 + b2;
        const float c2 = b1 - b2;
        const float c3 = b0 - b3;
        const float c4 = b4;
        const float c5 = b5 - b4;
        const float c6 = b6 - c5;
        const float c7 = b7 - b6;

        const float d0 = c0 + c1;
        const float d1 = c0 - c1;
        const float d2 = c2;
        const float d3 = c3 - c2;
        const float d4 = c4;
        const float d5 = c5;
        const float d6 = c6;
        const float d7 = c5 + c7;
        const float d8 = c4 - c6;

        const float e0 = d0;
        const float e1 = d1;
        const float e2 = d2 * m1;
        const float e3 = d3;
        const float e4 = d4 * m2;
        const float e5 = d5 * m3;
        const float e6 = d6 * m4;
        const float e7 = d7;
        const float e8 = d8 * m5;

        const float f0 = e0;
        const float f1 = e1;
        const float f2 = e2 + e3;
        const float f3 = e3 - e2;
        const float f4 = e4 + e8;
        const float f5 = e5 + e7;
        const float f6 = e6 + e8;
        const float f7 = e7 - e5;

        const float g0 = f0;
        const float g1 = f1;
        const float g2 = f2;
        const float g3 = f3;
        const float g4 = f4 + f7;
        const float g5 = f5 + f6;
        const float g6 = f5 - f6;
        const float g7 = f7 - f4;

        component[i * 8 + 0] = g0 * s0;
        component[i * 8 + 4] = g1 * s4;
        component[i * 8 + 2] = g2 * s2;
        component[i * 8 + 6] = g3 * s6;
        component[i * 8 + 5] = g4 * s5;
        component[i * 8 + 1] = g5 * s1;
        component[i * 8 + 7] = g6 * s7;
        component[i * 8 + 3] = g7 * s3;
    }
}

/* Perform FDCT on all MCUs */
static void forwardDCT(const BMPImage* image, const Header* ctx) {
    for (uint y = 0; y < ctx->blockHeightReal; ++y) {
        for (uint x = 0; x < ctx->blockWidthReal; ++x) {
            for (uint i = 0; i < 3; ++i) {
                forwardDCTBlockComponent(getMCUComponent(&image->blocks[y * ctx->blockWidthReal + x], i));
            }
        }
    }
}

/* ============================================== */
/*       Stage 4: Quantization                    */
/* ============================================== */

/* Quantize a block component based on a quantization table */
static void quantizeBlockComponent(const QuantizationTable* qTable, int* component) {
    for (uint i = 0; i < 64; ++i) {
        component[i] /= (int)qTable->table[i];
    }
}

/* Quantize all MCUs */
static void quantize(const BMPImage* image, const Header* ctx) {
    for (uint y = 0; y < ctx->blockHeightReal; ++y) {
        for (uint x = 0; x < ctx->blockWidthReal; ++x) {
            for (uint i = 0; i < 3; ++i) {
                quantizeBlockComponent(qTables100[i], getMCUComponent(&image->blocks[y * ctx->blockWidthReal + x], i));
            }
        }
    }
}

/* ============================================== */
/*       Stage 5: Huffman Encoding                */
/* ============================================== */

/* BitWriter structure */
typedef struct {
    byte nextBit;
    ByteArray* data;
} BitWriter;

static void bitwriter_init(BitWriter* writer, ByteArray* data) {
    writer->nextBit = 0;
    writer->data = data;
}

static void bitwriter_writeBit(BitWriter* writer, uint bit) {
    if (writer->nextBit == 0) {
        bytearray_push(writer->data, 0);
    }
    writer->data->data[writer->data->size - 1] |= (bit & 1) << (7 - writer->nextBit);
    writer->nextBit = (writer->nextBit + 1) % 8;
    if (writer->nextBit == 0 && writer->data->data[writer->data->size - 1] == 0xFF) {
        bytearray_push(writer->data, 0);
    }
}

static void bitwriter_writeBits(BitWriter* writer, uint bits, uint length) {
    for (uint i = 1; i <= length; ++i) {
        bitwriter_writeBit(writer, bits >> (length - i));
    }
}

/* Generate all Huffman codes based on symbols from a Huffman table */
static void generateCodes(HuffmanTable* hTable) {
    uint code = 0;
    for (uint i = 0; i < 16; ++i) {
        for (uint j = hTable->offsets[i]; j < hTable->offsets[i + 1]; ++j) {
            hTable->codes[j] = code;
            code += 1;
        }
        code <<= 1;
    }
}

static uint bitLength(int v) {
    uint length = 0;
    while (v > 0) {
        v >>= 1;
        length += 1;
    }
    return length;
}

static int getCode(const HuffmanTable* hTable, byte symbol, uint* code, uint* codeLength) {
    for (uint i = 0; i < 16; ++i) {
        for (uint j = hTable->offsets[i]; j < hTable->offsets[i + 1]; ++j) {
            if (symbol == hTable->symbols[j]) {
                *code = hTable->codes[j];
                *codeLength = i + 1;
                return 1;
            }
        }
    }
    return 0;
}

static int encodeBlockComponent(
    BitWriter* bitWriter,
    int* component,
    int* previousDC,
    const HuffmanTable* dcTable,
    const HuffmanTable* acTable
) {
    /* encode DC value */
    int coeff = component[0] - *previousDC;
    *previousDC = component[0];

    uint coeffLength = bitLength(abs(coeff));
    if (coeffLength > 11) {
        printf("Error - DC coefficient length greater than 11\n");
        return 0;
    }
    if (coeff < 0) {
        coeff += (1 << coeffLength) - 1;
    }

    uint code = 0;
    uint codeLength = 0;
    if (!getCode(dcTable, coeffLength, &code, &codeLength)) {
        printf("Error - Invalid DC value\n");
        return 0;
    }
    bitwriter_writeBits(bitWriter, code, codeLength);
    bitwriter_writeBits(bitWriter, coeff, coeffLength);

    /* encode AC values */
    for (uint i = 1; i < 64; ++i) {
        /* find zero run length */
        byte numZeroes = 0;
        while (i < 64 && component[zigZagMap[i]] == 0) {
            numZeroes += 1;
            i += 1;
        }

        if (i == 64) {
            if (!getCode(acTable, 0x00, &code, &codeLength)) {
                printf("Error - Invalid AC value\n");
                return 0;
            }
            bitwriter_writeBits(bitWriter, code, codeLength);
            return 1;
        }

        while (numZeroes >= 16) {
            if (!getCode(acTable, 0xF0, &code, &codeLength)) {
                printf("Error - Invalid AC value\n");
                return 0;
            }
            bitwriter_writeBits(bitWriter, code, codeLength);
            numZeroes -= 16;
        }

        /* find coeff length */
        coeff = component[zigZagMap[i]];
        coeffLength = bitLength(abs(coeff));
        if (coeffLength > 10) {
            printf("Error - AC coefficient length greater than 10\n");
            return 0;
        }
        if (coeff < 0) {
            coeff += (1 << coeffLength) - 1;
        }

        /* find symbol in table */
        byte symbol = numZeroes << 4 | coeffLength;
        if (!getCode(acTable, symbol, &code, &codeLength)) {
            printf("Error - Invalid AC value\n");
            return 0;
        }
        bitwriter_writeBits(bitWriter, code, codeLength);
        bitwriter_writeBits(bitWriter, coeff, coeffLength);
    }

    return 1;
}

/* Internal function to encode Huffman data for a single scan */
static int encodeHuffmanDataScan(BitWriter* bitWriter, const BMPImage* image, const Header* ctx, int* previousDCs) {
    const uint vSamp = ctx->verticalSamplingFactor;
    const uint hSamp = ctx->horizontalSamplingFactor;
    
    /* Iterate through MCU groups */
    for (uint y = 0; y < ctx->blockHeightReal; y += vSamp) {
        for (uint x = 0; x < ctx->blockWidthReal; x += hSamp) {
            /* Encode all Y blocks in this MCU group */
            for (uint v = 0; v < vSamp; ++v) {
                for (uint h = 0; h < hSamp; ++h) {
                    const uint blockY = y + v;
                    const uint blockX = x + h;
                    if (blockY < ctx->blockHeightReal && blockX < ctx->blockWidthReal) {
                        if (!encodeBlockComponent(
                                bitWriter,
                                getMCUComponent(&image->blocks[blockY * ctx->blockWidthReal + blockX], 0), /* Y component */
                                &previousDCs[0],
                                dcTables[0],
                                acTables[0])) {
                            return 0;
                        }
                    }
                }
            }
            
            /* Encode Cb block (from top-left block of MCU group) */
            if (!encodeBlockComponent(
                    bitWriter,
                    getMCUComponent(&image->blocks[y * ctx->blockWidthReal + x], 1), /* Cb component */
                    &previousDCs[1],
                    dcTables[1],
                    acTables[1])) {
                return 0;
            }
            
            /* Encode Cr block (from top-left block of MCU group) */
            if (!encodeBlockComponent(
                    bitWriter,
                    getMCUComponent(&image->blocks[y * ctx->blockWidthReal + x], 2), /* Cr component */
                    &previousDCs[2],
                    dcTables[2],
                    acTables[2])) {
                return 0;
            }
        }
    }
    return 1;
}

/* Stage 5: Huffman Encoding - encode all the Huffman data from all MCUs */
static ByteArray huffmanEncoding(const BMPImage* image, const Header* ctx) {
    ByteArray huffmanData;
    bytearray_init(&huffmanData);
    
    BitWriter bitWriter;
    bitwriter_init(&bitWriter, &huffmanData);

    int previousDCs[3] = { 0, 0, 0 };

    /* Generate Huffman codes for all tables (if not already generated) */
    for (uint i = 0; i < 3; ++i) {
        if (!dcTables[i]->set) {
            generateCodes(dcTables[i]);
            dcTables[i]->set = 1;
        }
        if (!acTables[i]->set) {
            generateCodes(acTables[i]);
            acTables[i]->set = 1;
        }
    }

    /* Encode the scan */
    if (!encodeHuffmanDataScan(&bitWriter, image, ctx, previousDCs)) {
        bytearray_free(&huffmanData);
        bytearray_init(&huffmanData);
        return huffmanData;
    }

    return huffmanData;
}

/* ============================================== */
/*       Stage 6: Write JPG                       */
/* ============================================== */

/* Helper function to write a 2-byte short integer in big-endian */
static void putShort(FILE* outFile, const uint v) {
    fputc((v >> 8) & 0xFF, outFile);
    fputc((v >> 0) & 0xFF, outFile);
}

static void writeQuantizationTable(FILE* outFile, byte tableID, const QuantizationTable* qTable) {
    fputc(0xFF, outFile);
    fputc(DQT, outFile);
    putShort(outFile, 67);
    fputc(tableID, outFile);
    for (uint i = 0; i < 64; ++i) {
        fputc(qTable->table[zigZagMap[i]], outFile);
    }
}

static void writeStartOfFrame(FILE* outFile, const BMPImage* image, const Header* ctx) {
    fputc(0xFF, outFile);
    fputc(SOF0, outFile);
    putShort(outFile, 17);
    fputc(8, outFile);
    putShort(outFile, image->height);
    putShort(outFile, image->width);
    fputc(3, outFile);
    for (uint i = 1; i <= 3; ++i) {
        fputc(i, outFile);
        /* Y component gets the full sampling factor, Cb/Cr get 1x1 */
        if (i == 1) {
            fputc((ctx->horizontalSamplingFactor << 4) | ctx->verticalSamplingFactor, outFile);
        } else {
            fputc(0x11, outFile); /* 1x1 for Cb and Cr */
        }
        fputc(i == 1 ? 0 : 1, outFile);
    }
}

static void writeHuffmanTable(FILE* outFile, byte acdc, byte tableID, const HuffmanTable* hTable) {
    fputc(0xFF, outFile);
    fputc(DHT, outFile);
    putShort(outFile, 19 + hTable->offsets[16]);
    fputc(acdc << 4 | tableID, outFile);
    for (uint i = 0; i < 16; ++i) {
        fputc(hTable->offsets[i + 1] - hTable->offsets[i], outFile);
    }
    for (uint i = 0; i < 16; ++i) {
        for (uint j = hTable->offsets[i]; j < hTable->offsets[i + 1]; ++j) {
            fputc(hTable->symbols[j], outFile);
        }
    }
}

static void writeStartOfScan(FILE* outFile) {
    fputc(0xFF, outFile);
    fputc(SOS, outFile);
    putShort(outFile, 12);
    fputc(3, outFile);
    for (uint i = 1; i <= 3; ++i) {
        fputc(i, outFile);
        fputc(i == 1 ? 0x00 : 0x11, outFile);
    }
    fputc(0, outFile);
    fputc(63, outFile);
    fputc(0, outFile);
}

static void writeAPP0(FILE* outFile) {
    fputc(0xFF, outFile);
    fputc(APP0, outFile);
    putShort(outFile, 16);
    fputc('J', outFile);
    fputc('F', outFile);
    fputc('I', outFile);
    fputc('F', outFile);
    fputc(0, outFile);
    fputc(1, outFile);
    fputc(2, outFile);
    fputc(0, outFile);
    putShort(outFile, 100);
    putShort(outFile, 100);
    fputc(0, outFile);
    fputc(0, outFile);
}

/* Stage 6: Write JPG - write the JPEG file */
static void writeJPG(const BMPImage* image, const Header* ctx, const ByteArray* huffmanData, const char* filename) {
    if (huffmanData->size == 0) {
        printf("Error - No Huffman data to write\n");
        return;
    }

    /* open file */
    printf("Writing %s...\n", filename);
    FILE* outFile = fopen(filename, "wb");
    if (outFile == NULL) {
        printf("Error - Error opening output file\n");
        return;
    }

    /* SOI */
    fputc(0xFF, outFile);
    fputc(SOI, outFile);

    /* APP0 */
    writeAPP0(outFile);

    /* DQT */
    writeQuantizationTable(outFile, 0, &qTableY100);
    writeQuantizationTable(outFile, 1, &qTableCbCr100);

    /* SOF */
    writeStartOfFrame(outFile, image, ctx);

    /* DHT */
    writeHuffmanTable(outFile, 0, 0, &hDCTableY);
    writeHuffmanTable(outFile, 0, 1, &hDCTableCbCr);
    writeHuffmanTable(outFile, 1, 0, &hACTableY);
    writeHuffmanTable(outFile, 1, 1, &hACTableCbCr);

    /* SOS */
    writeStartOfScan(outFile);

    /* ECS */
    fwrite(huffmanData->data, 1, huffmanData->size, outFile);

    /* EOI */
    fputc(0xFF, outFile);
    fputc(EOI, outFile);

    fclose(outFile);
}

/* ============================================== */
/*                   Main                         */
/* ============================================== */

int main(int argc, char** argv) {
    /* validate arguments */
    if (argc < 2) {
        printf("Usage: encoder_c <bmp_file> [sampling]\n");
        printf("  sampling: 1x1, 2x1, 1x2, or 2x2 (default: 1x1)\n");
        return 1;
    }

    /* Parse sampling factor from command line (default 1x1) */
    byte hSamp = 1, vSamp = 1;
    if (argc >= 3) {
        const char* sampArg = argv[argc - 1];
        if (strcmp(sampArg, "2x1") == 0) {
            hSamp = 2; vSamp = 1;
        } else if (strcmp(sampArg, "1x2") == 0) {
            hSamp = 1; vSamp = 2;
        } else if (strcmp(sampArg, "2x2") == 0) {
            hSamp = 2; vSamp = 2;
        } else if (strcmp(sampArg, "1x1") != 0) {
            /* Not a sampling argument, treat as filename */
            hSamp = 1; vSamp = 1;
        }
    }

    int numFiles = (argc >= 3 && (strcmp(argv[argc - 1], "1x1") == 0 || 
                                   strcmp(argv[argc - 1], "2x1") == 0 || 
                                   strcmp(argv[argc - 1], "1x2") == 0 || 
                                   strcmp(argv[argc - 1], "2x2") == 0)) ? argc - 2 : argc - 1;

    for (int i = 1; i <= numFiles; ++i) {
        const char* filename = argv[i];
        
        struct timespec start, end;
        double stage_times[7] = {0};
        double total_time = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);

        /* Stage 1: Read BMP image */
        BMPImage image = readBMP(filename);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[0] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        /* validate image */
        if (image.blocks == NULL) {
            continue;
        }

        /* Create encoder context with sampling factors */
        Header ctx;
        memset(&ctx, 0, sizeof(Header));
        ctx.horizontalSamplingFactor = hSamp;
        ctx.verticalSamplingFactor = vSamp;
        ctx.valid = 1;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 2: Color Conversion */
        RGBToYCbCr(&image);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[1] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 2.5: Downsample */
        downsample(&image, &ctx);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[2] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 3: Forward Discrete Cosine Transform */
        forwardDCT(&image, &ctx);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[3] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 4: Quantize DCT coefficients */
        quantize(&image, &ctx);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[4] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 5: Huffman Encoding */
        ByteArray huffmanData = huffmanEncoding(&image, &ctx);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[5] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        if (huffmanData.size == 0) {
            free(image.blocks);
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 6: Write JPG file */
        /* Construct output filename */
        char outFilename[512];
        const char* dot = strrchr(filename, '.');
        if (dot != NULL) {
            size_t baseLen = dot - filename;
            strncpy(outFilename, filename, baseLen);
            outFilename[baseLen] = '\0';
            strcat(outFilename, "_encoder_c_out.jpg");
        } else {
            strcpy(outFilename, filename);
            strcat(outFilename, "_encoder_c_out.jpg");
        }
        writeJPG(&image, &ctx, &huffmanData, outFilename);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[6] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        /* Calculate and print timing statistics */
        total_time = 0;
        for (int j = 0; j < 7; j++) {
            total_time += stage_times[j];
        }
        
        printf("\n=== Encoder Timing Statistics ===\n");
        printf("Stage 1 (Read BMP):            %8.4f ms\n", stage_times[0] * 1000);
        printf("Stage 2 (Color Conversion):    %8.4f ms\n", stage_times[1] * 1000);
        printf("Stage 3 (Downsampling):        %8.4f ms\n", stage_times[2] * 1000);
        printf("Stage 4 (Forward DCT):         %8.4f ms\n", stage_times[3] * 1000);
        printf("Stage 5 (Quantization):        %8.4f ms\n", stage_times[4] * 1000);
        printf("Stage 6 (Huffman Encoding):    %8.4f ms\n", stage_times[5] * 1000);
        printf("Stage 7 (Write JPG):           %8.4f ms\n", stage_times[6] * 1000);
        printf("----------------------------------\n");
        printf("Total Time:                    %8.4f ms\n", total_time * 1000);
        printf("==================================\n\n");

        bytearray_free(&huffmanData);
        free(image.blocks);
    }
    return 0;
}
