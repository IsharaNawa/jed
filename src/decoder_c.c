#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jpg_c.h"

/* BitReader structure */
typedef struct {
    byte nextByte;
    byte nextBit;
    FILE* inFile;
} BitReader;

BitReader* bitreader_create(const char* filename) {
    BitReader* reader = (BitReader*)malloc(sizeof(BitReader));
    if (reader == NULL) return NULL;
    
    reader->nextByte = 0;
    reader->nextBit = 0;
    reader->inFile = fopen(filename, "rb");
    
    if (reader->inFile == NULL) {
        free(reader);
        return NULL;
    }
    
    return reader;
}

void bitreader_destroy(BitReader* reader) {
    if (reader != NULL) {
        if (reader->inFile != NULL) {
            fclose(reader->inFile);
        }
        free(reader);
    }
}

int bitreader_hasbits(BitReader* reader) {
    return !feof(reader->inFile);
}

byte bitreader_readbyte(BitReader* reader) {
    reader->nextBit = 0;
    return fgetc(reader->inFile);
}

uint bitreader_readword(BitReader* reader) {
    reader->nextBit = 0;
    byte b1 = fgetc(reader->inFile);
    byte b2 = fgetc(reader->inFile);
    return (b1 << 8) + b2;
}

uint bitreader_readbit(BitReader* reader) {
    if (reader->nextBit == 0) {
        if (!bitreader_hasbits(reader)) {
            return (uint)-1;
        }
        reader->nextByte = fgetc(reader->inFile);
        while (reader->nextByte == 0xFF) {
            byte nextByteCheck = fgetc(reader->inFile);
            if (nextByteCheck != 0x00) {
                if ((nextByteCheck >= RST0 && nextByteCheck <= RST7) || nextByteCheck == 0xFF) {
                    reader->nextByte = fgetc(reader->inFile);
                } else {
                    reader->nextByte = nextByteCheck;
                    break;
                }
            }
        }
    }
    uint bit = (reader->nextByte >> (7 - reader->nextBit)) & 1;
    reader->nextBit = (reader->nextBit + 1) % 8;
    return bit;
}

uint bitreader_readbits(BitReader* reader, const uint length) {
    uint bits = 0;
    for (uint i = 0; i < length; ++i) {
        uint bit = bitreader_readbit(reader);
        if (bit == (uint)-1) {
            return (uint)-1;
        }
        bits = (bits << 1) | bit;
    }
    return bits;
}

void bitreader_align(BitReader* reader) {
    reader->nextBit = 0;
}

/* BitReaderFromMemory structure */
typedef struct {
    byte nextByte;
    byte nextBit;
    const ByteArray* data;
    size_t position;
} BitReaderFromMemory;

void bitreader_mem_init(BitReaderFromMemory* reader, const ByteArray* data) {
    reader->nextByte = 0;
    reader->nextBit = 0;
    reader->data = data;
    reader->position = 0;
}

int bitreader_mem_hasbits(BitReaderFromMemory* reader) {
    return reader->position < reader->data->size;
}

byte bitreader_mem_readbyte(BitReaderFromMemory* reader) {
    reader->nextBit = 0;
    if (reader->position < reader->data->size) {
        return reader->data->data[reader->position++];
    }
    return 0;
}

uint bitreader_mem_readword(BitReaderFromMemory* reader) {
    reader->nextBit = 0;
    uint word = 0;
    if (reader->position < reader->data->size) {
        word = reader->data->data[reader->position++] << 8;
    }
    if (reader->position < reader->data->size) {
        word += reader->data->data[reader->position++];
    }
    return word;
}

byte bitreader_mem_peekbyte(BitReaderFromMemory* reader) {
    if (reader->position < reader->data->size) {
        return reader->data->data[reader->position];
    }
    return 0;
}

uint bitreader_mem_readbit(BitReaderFromMemory* reader) {
    if (reader->nextBit == 0) {
        if (!bitreader_mem_hasbits(reader)) {
            return (uint)-1;
        }
        reader->nextByte = bitreader_mem_readbyte(reader);
        while (reader->nextByte == 0xFF) {
            byte nextByteCheck = bitreader_mem_peekbyte(reader);
            if (nextByteCheck != 0x00) {
                if ((nextByteCheck >= RST0 && nextByteCheck <= RST7) || nextByteCheck == 0xFF) {
                    bitreader_mem_readbyte(reader);
                    if (bitreader_mem_hasbits(reader)) {
                        reader->nextByte = bitreader_mem_readbyte(reader);
                    }
                } else {
                    reader->nextByte = bitreader_mem_readbyte(reader);
                    break;
                }
            } else {
                bitreader_mem_readbyte(reader);
                break;
            }
        }
    }
    uint bit = (reader->nextByte >> (7 - reader->nextBit)) & 1;
    reader->nextBit = (reader->nextBit + 1) % 8;
    return bit;
}

uint bitreader_mem_readbits(BitReaderFromMemory* reader, const uint length) {
    uint bits = 0;
    for (uint i = 0; i < length; ++i) {
        uint bit = bitreader_mem_readbit(reader);
        if (bit == (uint)-1) {
            return (uint)-1;
        }
        bits = (bits << 1) | bit;
    }
    return bits;
}

void bitreader_mem_align(BitReaderFromMemory* reader) {
    reader->nextBit = 0;
}

/* Forward declarations */
void generateCodes(HuffmanTable* hTable);
byte getNextSymbol_file(BitReader* bitReader, const HuffmanTable* hTable);
byte getNextSymbol_mem(BitReaderFromMemory* bitReader, const HuffmanTable* hTable);

/* SOF - Start of Frame */
void readStartOfFrame(BitReader* bitReader, Header* image) {
    printf("Reading SOF Marker\n");
    if (image->numComponents != 0) {
        printf("Error - Multiple SOFs detected\n");
        image->valid = 0;
        return;
    }

    uint length = bitreader_readword(bitReader);
    byte precision = bitreader_readbyte(bitReader);
    
    if (precision != 8) {
        printf("Error - Invalid precision: %u\n", (uint)precision);
        image->valid = 0;
        return;
    }

    image->height = bitreader_readword(bitReader);
    image->width = bitreader_readword(bitReader);
    
    if (image->height == 0 || image->width == 0) {
        printf("Error - Invalid dimensions\n");
        image->valid = 0;
        return;
    }
    
    image->blockHeight = (image->height + 7) / 8;
    image->blockWidth = (image->width + 7) / 8;
    image->blockHeightReal = image->blockHeight;
    image->blockWidthReal = image->blockWidth;

    image->numComponents = bitreader_readbyte(bitReader);
    
    if (image->numComponents == 4) {
        printf("Error - CMYK color mode not supported\n");
        image->valid = 0;
        return;
    }
    
    if (image->numComponents != 1 && image->numComponents != 3) {
        printf("Error - %u color components given (1 or 3 required)\n", (uint)image->numComponents);
        image->valid = 0;
        return;
    }

    for (uint i = 0; i < image->numComponents; ++i) {
        byte componentID = bitreader_readbyte(bitReader);
        
        if (componentID == 0 && i == 0) {
            image->zeroBased = 1;
        }
        if (image->zeroBased) {
            componentID += 1;
        }
        if (componentID == 0 || componentID > image->numComponents) {
            printf("Error - Invalid component ID: %u\n", (uint)componentID);
            image->valid = 0;
            return;
        }
        
        ColorComponent* component = &image->colorComponents[componentID - 1];
        if (component->usedInFrame) {
            printf("Error - Duplicate color component ID: %u\n", (uint)componentID);
            image->valid = 0;
            return;
        }
        component->usedInFrame = 1;

        byte samplingFactor = bitreader_readbyte(bitReader);
        component->horizontalSamplingFactor = samplingFactor >> 4;
        component->verticalSamplingFactor = samplingFactor & 0x0F;
        
        if (componentID == 1) {
            if ((component->horizontalSamplingFactor != 1 && component->horizontalSamplingFactor != 2) ||
                (component->verticalSamplingFactor != 1 && component->verticalSamplingFactor != 2)) {
                printf("Error - Invalid sampling factor\n");
                image->valid = 0;
                return;
            }
            if (component->horizontalSamplingFactor == 2 && image->blockWidth % 2 == 1) {
                image->blockWidthReal += 1;
            }
            if (component->verticalSamplingFactor == 2 && image->blockHeight % 2 == 1) {
                image->blockHeightReal += 1;
            }
            image->horizontalSamplingFactor = component->horizontalSamplingFactor;
            image->verticalSamplingFactor = component->verticalSamplingFactor;
        } else {
            if (component->horizontalSamplingFactor != 1 || component->verticalSamplingFactor != 1) {
                printf("Error - Invalid sampling factor\n");
                image->valid = 0;
                return;
            }
        }

        component->quantizationTableID = bitreader_readbyte(bitReader);
        if (component->quantizationTableID > 3) {
            printf("Error - Invalid quantization table ID: %u\n", (uint)component->quantizationTableID);
            image->valid = 0;
            return;
        }
    }

    if (length - 8 - (3 * image->numComponents) != 0) {
        printf("Error - SOF invalid\n");
        image->valid = 0;
        return;
    }
}

/* DQT - Quantization Table */
void readQuantizationTable(BitReader* bitReader, Header* image) {
    printf("Reading DQT Marker\n");
    int length = bitreader_readword(bitReader);
    length -= 2;

    while (length > 0) {
        byte tableInfo = bitreader_readbyte(bitReader);
        length -= 1;
        byte tableID = tableInfo & 0x0F;

        if (tableID > 3) {
            printf("Error - Invalid quantization table ID: %u\n", (uint)tableID);
            image->valid = 0;
            return;
        }
        
        QuantizationTable* qTable = &image->quantizationTables[tableID];
        qTable->set = 1;

        if (tableInfo >> 4 != 0) {
            for (uint i = 0; i < 64; ++i) {
                qTable->table[zigZagMap[i]] = bitreader_readword(bitReader);
            }
            length -= 128;
        } else {
            for (uint i = 0; i < 64; ++i) {
                qTable->table[zigZagMap[i]] = bitreader_readbyte(bitReader);
            }
            length -= 64;
        }
    }

    if (length != 0) {
        printf("Error - DQT invalid\n");
        image->valid = 0;
        return;
    }
}

/* Generate Huffman codes */
void generateCodes(HuffmanTable* hTable) {
    uint code = 0;
    for (uint i = 0; i < 16; ++i) {
        for (uint j = hTable->offsets[i]; j < hTable->offsets[i + 1]; ++j) {
            hTable->codes[j] = code;
            code += 1;
        }
        code <<= 1;
    }
}

/* DHT - Huffman Table */
void readHuffmanTable(BitReader* bitReader, Header* image) {
    printf("Reading DHT Marker\n");
    int length = bitreader_readword(bitReader);
    length -= 2;

    while (length > 0) {
        byte tableInfo = bitreader_readbyte(bitReader);
        byte tableID = tableInfo & 0x0F;
        int acTable = tableInfo >> 4;

        if (tableID > 3) {
            printf("Error - Invalid Huffman table ID: %u\n", (uint)tableID);
            image->valid = 0;
            return;
        }

        HuffmanTable* hTable = acTable ? &image->huffmanACTables[tableID] : &image->huffmanDCTables[tableID];
        hTable->set = 1;

        hTable->offsets[0] = 0;
        uint allSymbols = 0;
        for (uint i = 1; i <= 16; ++i) {
            allSymbols += bitreader_readbyte(bitReader);
            hTable->offsets[i] = allSymbols;
        }
        
        if (allSymbols > 176) {
            printf("Error - Too many symbols in Huffman table: %u\n", allSymbols);
            image->valid = 0;
            return;
        }

        for (uint i = 0; i < allSymbols; ++i) {
            hTable->symbols[i] = bitreader_readbyte(bitReader);
        }

        generateCodes(hTable);

        length -= 17 + allSymbols;
    }

    if (length != 0) {
        printf("Error - DHT invalid\n");
        image->valid = 0;
        return;
    }
}

/* SOS - Start of Scan */
void readStartOfScan(BitReader* bitReader, Header* image) {
    printf("Reading SOS Marker\n");
    if (image->numComponents == 0) {
        printf("Error - SOS detected before SOF\n");
        image->valid = 0;
        return;
    }

    uint length = bitreader_readword(bitReader);

    for (uint i = 0; i < image->numComponents; ++i) {
        image->colorComponents[i].usedInScan = 0;
    }

    image->componentsInScan = bitreader_readbyte(bitReader);
    if (image->componentsInScan == 0) {
        printf("Error - Scan must include at least 1 component\n");
        image->valid = 0;
        return;
    }
    
    for (uint i = 0; i < image->componentsInScan; ++i) {
        byte componentID = bitreader_readbyte(bitReader);
        
        if (image->zeroBased) {
            componentID += 1;
        }
        if (componentID == 0 || componentID > image->numComponents) {
            printf("Error - Invalid color component ID: %u\n", (uint)componentID);
            image->valid = 0;
            return;
        }
        
        ColorComponent* component = &image->colorComponents[componentID - 1];
        if (!component->usedInFrame) {
            printf("Error - Invalid color component ID: %u\n", (uint)componentID);
            image->valid = 0;
            return;
        }
        if (component->usedInScan) {
            printf("Error - Duplicate color component ID: %u\n", (uint)componentID);
            image->valid = 0;
            return;
        }
        component->usedInScan = 1;

        byte huffmanTableIDs = bitreader_readbyte(bitReader);
        component->huffmanDCTableID = huffmanTableIDs >> 4;
        component->huffmanACTableID = huffmanTableIDs & 0x0F;
        
        if (component->huffmanDCTableID > 3) {
            printf("Error - Invalid Huffman DC table ID: %u\n", (uint)component->huffmanDCTableID);
            image->valid = 0;
            return;
        }
        if (component->huffmanACTableID > 3) {
            printf("Error - Invalid Huffman AC table ID: %u\n", (uint)component->huffmanACTableID);
            image->valid = 0;
            return;
        }
    }

    image->startOfSelection = bitreader_readbyte(bitReader);
    image->endOfSelection = bitreader_readbyte(bitReader);
    byte successiveApproximation = bitreader_readbyte(bitReader);
    image->successiveApproximationHigh = successiveApproximation >> 4;
    image->successiveApproximationLow = successiveApproximation & 0x0F;

    if (image->frameType == SOF0) {
        if (image->startOfSelection != 0 || image->endOfSelection != 63) {
            printf("Error - Invalid spectral selection\n");
            image->valid = 0;
            return;
        }
        if (image->successiveApproximationHigh != 0 || image->successiveApproximationLow != 0) {
            printf("Error - Invalid successive approximation\n");
            image->valid = 0;
            return;
        }
    }

    for (uint i = 0; i < image->numComponents; ++i) {
        const ColorComponent* component = &image->colorComponents[i];
        if (component->usedInScan) {
            if (!image->huffmanDCTables[component->huffmanDCTableID].set) {
                printf("Error - Huffman DC table not set\n");
                image->valid = 0;
                return;
            }
            if (!image->huffmanACTables[component->huffmanACTableID].set) {
                printf("Error - Huffman AC table not set\n");
                image->valid = 0;
                return;
            }
            if (!image->quantizationTables[component->quantizationTableID].set) {
                printf("Error - Quantization table not set\n");
                image->valid = 0;
                return;
            }
        }
    }

    if (length - 6 - (2 * image->componentsInScan) != 0) {
        printf("Error - SOS invalid\n");
        image->valid = 0;
        return;
    }
}

/* DRI - Restart Interval */
void readRestartInterval(BitReader* bitReader, Header* image) {
    printf("Reading DRI Marker\n");
    uint length = bitreader_readword(bitReader);
    image->restartInterval = bitreader_readword(bitReader);
    
    if (length - 4 != 0) {
        printf("Error - DRI invalid\n");
        image->valid = 0;
        return;
    }
}

/* APPN markers */
void readAPPN(BitReader* bitReader, Header* image) {
    printf("Reading APPN Marker\n");
    uint length = bitreader_readword(bitReader);
    
    if (length < 2) {
        printf("Error - APPN invalid\n");
        image->valid = 0;
        return;
    }

    for (uint i = 0; i < length - 2; ++i) {
        bitreader_readbyte(bitReader);
    }
}

/* Comment markers */
void readComment(BitReader* bitReader, Header* image) {
    printf("Reading COM Marker\n");
    uint length = bitreader_readword(bitReader);
    
    if (length < 2) {
        printf("Error - COM invalid\n");
        image->valid = 0;
        return;
    }

    for (uint i = 0; i < length - 2; ++i) {
        bitreader_readbyte(bitReader);
    }
}

/* Print frame info */
void printFrameInfo(const Header* image) {
    if (image == NULL) return;
    
    printf("SOF=============\n");
    printf("Frame Type: 0x%02x\n", (uint)image->frameType);
    printf("Height: %u\n", image->height);
    printf("Width: %u\n", image->width);
    printf("Color Components:\n");
    
    for (uint i = 0; i < image->numComponents; ++i) {
        if (image->colorComponents[i].usedInFrame) {
            printf("Component ID: %u\n", i + 1);
            printf("Horizontal Sampling Factor: %u\n", (uint)image->colorComponents[i].horizontalSamplingFactor);
            printf("Vertical Sampling Factor: %u\n", (uint)image->colorComponents[i].verticalSamplingFactor);
            printf("Quantization Table ID: %u\n", (uint)image->colorComponents[i].quantizationTableID);
        }
    }
    
    printf("DQT=============\n");
    for (uint i = 0; i < 4; ++i) {
        if (image->quantizationTables[i].set) {
            printf("Table ID: %u\n", i);
        }
    }
}

/* Print scan info */
void printScanInfo(const Header* image) {
    if (image == NULL) return;
    
    printf("SOS=============\n");
    printf("Start of Selection: %u\n", (uint)image->startOfSelection);
    printf("End of Selection: %u\n", (uint)image->endOfSelection);
    printf("Successive Approximation High: %u\n", (uint)image->successiveApproximationHigh);
    printf("Successive Approximation Low: %u\n", (uint)image->successiveApproximationLow);
    printf("Color Components:\n");
    
    for (uint i = 0; i < image->numComponents; ++i) {
        if (image->colorComponents[i].usedInScan) {
            printf("Component ID: %u\n", i + 1);
            printf("Huffman DC Table ID: %u\n", (uint)image->colorComponents[i].huffmanDCTableID);
            printf("Huffman AC Table ID: %u\n", (uint)image->colorComponents[i].huffmanACTableID);
        }
    }
    
    printf("DHT=============\n");
    printf("DC Tables:\n");
    for (uint i = 0; i < 4; ++i) {
        if (image->huffmanDCTables[i].set) {
            printf("Table ID: %u\n", i);
        }
    }
    printf("AC Tables:\n");
    for (uint i = 0; i < 4; ++i) {
        if (image->huffmanACTables[i].set) {
            printf("Table ID: %u\n", i);
        }
    }
    
    printf("DRI=============\n");
    printf("Restart Interval: %u\n", image->restartInterval);
}

/* Read frame header */
void readFrameHeader(BitReader* bitReader, Header* image) {
    byte last = bitreader_readbyte(bitReader);
    byte current = bitreader_readbyte(bitReader);
    
    if (last != 0xFF || current != SOI) {
        printf("Error - SOI invalid\n");
        image->valid = 0;
        return;
    }
    
    last = bitreader_readbyte(bitReader);
    current = bitreader_readbyte(bitReader);

    while (image->valid) {
        if (!bitreader_hasbits(bitReader)) {
            printf("Error - File ended prematurely\n");
            image->valid = 0;
            return;
        }
        if (last != 0xFF) {
            printf("Error - Expected a marker\n");
            image->valid = 0;
            return;
        }

        if (current == SOF0) {
            image->frameType = SOF0;
            readStartOfFrame(bitReader, image);
        } else if (current == SOF2) {
            image->frameType = SOF2;
            readStartOfFrame(bitReader, image);
        } else if (current == DQT) {
            readQuantizationTable(bitReader, image);
        } else if (current == DHT) {
            readHuffmanTable(bitReader, image);
        } else if (current == DRI) {
            readRestartInterval(bitReader, image);
        } else if (current == SOS) {
            return;
        } else if (current >= APP0 && current <= APP15) {
            readAPPN(bitReader, image);
        } else if (current == COM) {
            readComment(bitReader, image);
        } else if ((current >= JPG0 && current <= JPG13) || current == DNL || current == DHP || current == EXP) {
            printf("Error - Unsupported marker: 0x%02x\n", current);
            image->valid = 0;
            return;
        } else if (current == TEM) {
            /* TEM has no size */
        } else if (current == 0xFF) {
            /* Extra 0xFF padding byte, skip it */
            current = bitreader_readbyte(bitReader);
            continue;
        } else {
            printf("Error - Unknown marker: 0x%02x\n", current);
            image->valid = 0;
            return;
        }
        
        last = bitreader_readbyte(bitReader);
        current = bitreader_readbyte(bitReader);
    }
}

/* Read scans */
void readScans(BitReader* bitReader, Header* image) {
    readStartOfScan(bitReader, image);
    if (!image->valid) {
        return;
    }
    printScanInfo(image);
    
    /* Read all remaining bytes for Huffman decoding stage */
    while (bitreader_hasbits(bitReader)) {
        byte b = bitreader_readbyte(bitReader);
        bytearray_push(&image->huffmanData, b);
    }
}

/* Stage 1: Read JPEG */
Header* readJPEG(const char* filename) {
    printf("Reading %s...\n", filename);
    
    BitReader* bitReader = bitreader_create(filename);
    if (bitReader == NULL) {
        printf("Error - Error opening input file\n");
        return NULL;
    }

    Header* image = (Header*)calloc(1, sizeof(Header));
    if (image == NULL) {
        printf("Error - Memory error\n");
        bitreader_destroy(bitReader);
        return NULL;
    }
    
    image->valid = 1;
    image->filename = (char*)malloc(strlen(filename) + 1);
    if (image->filename != NULL) {
        strcpy(image->filename, filename);
    }
    bytearray_init(&image->huffmanData);

    readFrameHeader(bitReader, image);

    if (!image->valid) {
        bitreader_destroy(bitReader);
        return image;
    }

    printFrameInfo(image);

    image->blocks = (MCU*)calloc(image->blockHeightReal * image->blockWidthReal, sizeof(MCU));
    if (image->blocks == NULL) {
        printf("Error - Memory error\n");
        image->valid = 0;
        bitreader_destroy(bitReader);
        return image;
    }

    readScans(bitReader, image);
    bitreader_destroy(bitReader);

    return image;
}

/* Get next symbol from Huffman table (file version) */
byte getNextSymbol_file(BitReader* bitReader, const HuffmanTable* hTable) {
    uint currentCode = 0;
    for (uint i = 0; i < 16; ++i) {
        uint bit = bitreader_readbit(bitReader);
        if (bit == (uint)-1) {
            return -1;
        }
        currentCode = (currentCode << 1) | bit;
        for (uint j = hTable->offsets[i]; j < hTable->offsets[i + 1]; ++j) {
            if (currentCode == hTable->codes[j]) {
                return hTable->symbols[j];
            }
        }
    }
    return -1;
}

/* Get next symbol from Huffman table (memory version) */
byte getNextSymbol_mem(BitReaderFromMemory* bitReader, const HuffmanTable* hTable) {
    uint currentCode = 0;
    for (uint i = 0; i < 16; ++i) {
        uint bit = bitreader_mem_readbit(bitReader);
        if (bit == (uint)-1) {
            return -1;
        }
        currentCode = (currentCode << 1) | bit;
        for (uint j = hTable->offsets[i]; j < hTable->offsets[i + 1]; ++j) {
            if (currentCode == hTable->codes[j]) {
                return hTable->symbols[j];
            }
        }
    }
    return -1;
}

/* Decode block component (for baseline) */
int decodeBlockComponent_baseline(
    const Header* image,
    BitReaderFromMemory* bitReader,
    int* component,
    int* previousDC,
    const HuffmanTable* dcTable,
    const HuffmanTable* acTable
) {
    byte length = getNextSymbol_mem(bitReader, dcTable);
    if (length == (byte)-1) {
        printf("Error - Invalid Huffman code\n");
        return 0;
    }
    if (length > 11) {
        printf("Error - Invalid DC code length\n");
        return 0;
    }

    int coeff = bitreader_mem_readbits(bitReader, length);
    if (coeff == (int)-1) {
        printf("Error - Invalid DC value\n");
        return 0;
    }
    if (length != 0 && coeff < (1 << (length - 1))) {
        coeff -= (1 << length) - 1;
    }

    component[0] = coeff + *previousDC;
    *previousDC = component[0];

    /* Get the AC values for this block component */
    for (uint i = 1; i < 64; ++i) {
        byte symbol = getNextSymbol_mem(bitReader, acTable);
        if (symbol == (byte)-1) {
            printf("Error - Invalid Huffman code\n");
            return 0;
        }

        /* symbol 0x00 means fill remainder of component with 0 (already zeroed by calloc) */
        if (symbol == 0x00) {
            return 1;
        }

        /* otherwise, read next component coefficient */
        byte numZeros = symbol >> 4;
        byte coeffLength = symbol & 0x0F;

        if (i + numZeros >= 64) {
            printf("Error - Zero run-length exceeded block component\n");
            return 0;
        }
        i += numZeros;

        if (coeffLength > 10) {
            printf("Error - AC coefficient length greater than 10\n");
            return 0;
        }
        coeff = bitreader_mem_readbits(bitReader, coeffLength);
        if (coeff == (int)-1) {
            printf("Error - Invalid AC value\n");
            return 0;
        }
        if (coeff < (1 << (coeffLength - 1))) {
            coeff -= (1 << coeffLength) - 1;
        }
        component[zigZagMap[i]] = coeff;
    }

    return 1;
}

/* Decode Huffman data for baseline JPEG */
void decodeHuffmanDataScan(BitReaderFromMemory* bitReader, Header* image, int* previousDCs) {
    const int luminanceOnly = image->componentsInScan == 1 && image->colorComponents[0].usedInScan;
    const uint yStep = luminanceOnly ? 1 : image->verticalSamplingFactor;
    const uint xStep = luminanceOnly ? 1 : image->horizontalSamplingFactor;
    const uint restartInterval = image->restartInterval * xStep * yStep;

    uint mcuCount = 0;
    
    for (uint y = 0; y < image->blockHeight; y += yStep) {
        for (uint x = 0; x < image->blockWidth; x += xStep) {
            if (restartInterval != 0 && mcuCount % restartInterval == 0 && mcuCount != 0) {
                previousDCs[0] = 0;
                previousDCs[1] = 0;
                previousDCs[2] = 0;
                bitreader_mem_align(bitReader);
            }
            
            for (uint v = 0; v < image->verticalSamplingFactor; ++v) {
                for (uint h = 0; h < image->horizontalSamplingFactor; ++h) {
                    MCU* block = &image->blocks[(y + v) * image->blockWidthReal + (x + h)];
                    
                    if (image->colorComponents[0].usedInScan) {
                        if (!decodeBlockComponent_baseline(
                            image,
                            bitReader,
                            block->y,
                            &previousDCs[0],
                            &image->huffmanDCTables[image->colorComponents[0].huffmanDCTableID],
                            &image->huffmanACTables[image->colorComponents[0].huffmanACTableID]
                        )) {
                            image->valid = 0;
                            return;
                        }
                    }
                }
            }
            
            if (image->numComponents == 3) {
                MCU* block = &image->blocks[y * image->blockWidthReal + x];
                
                if (image->colorComponents[1].usedInScan) {
                    if (!decodeBlockComponent_baseline(
                        image,
                        bitReader,
                        block->cb,
                        &previousDCs[1],
                        &image->huffmanDCTables[image->colorComponents[1].huffmanDCTableID],
                        &image->huffmanACTables[image->colorComponents[1].huffmanACTableID]
                    )) {
                        image->valid = 0;
                        return;
                    }
                }
                
                if (image->colorComponents[2].usedInScan) {
                    if (!decodeBlockComponent_baseline(
                        image,
                        bitReader,
                        block->cr,
                        &previousDCs[2],
                        &image->huffmanDCTables[image->colorComponents[2].huffmanDCTableID],
                        &image->huffmanACTables[image->colorComponents[2].huffmanACTableID]
                    )) {
                        image->valid = 0;
                        return;
                    }
                }
            }
            
            mcuCount += 1;
        }
    }
}

/* Stage 2: Huffman Decoding */
void huffmanDecoding(Header* image) {
    BitReaderFromMemory bitReader;
    bitreader_mem_init(&bitReader, &image->huffmanData);
    
    int previousDCs[3] = { 0, 0, 0 };

    decodeHuffmanDataScan(&bitReader, image, previousDCs);
    
    if (!image->valid) {
        return;
    }

    /* Handle progressive JPEG if needed */
    if (image->frameType == SOF2) {
        printf("Warning - Progressive JPEG not fully supported\n");
    }
}

/* Stage 3: Dequantization */
void dequantizeBlockComponent(const QuantizationTable* qTable, int* component) {
    for (uint i = 0; i < 64; ++i) {
        component[i] *= qTable->table[i];
    }
}

void dequantize(const Header* image) {
    for (uint y = 0; y < image->blockHeight; y += image->verticalSamplingFactor) {
        for (uint x = 0; x < image->blockWidth; x += image->horizontalSamplingFactor) {
            for (uint i = 0; i < image->numComponents; ++i) {
                const ColorComponent* component = &image->colorComponents[i];
                for (uint v = 0; v < component->verticalSamplingFactor; ++v) {
                    for (uint h = 0; h < component->horizontalSamplingFactor; ++h) {
                        MCU* block = &image->blocks[(y + v) * image->blockWidthReal + (x + h)];
                        int* componentData;
                        switch (i) {
                            case 0:
                                componentData = block->y;
                                break;
                            case 1:
                                componentData = block->cb;
                                break;
                            case 2:
                                componentData = block->cr;
                                break;
                            default:
                                componentData = NULL;
                                break;
                        }
                        if (componentData) {
                            dequantizeBlockComponent(&image->quantizationTables[component->quantizationTableID], componentData);
                        }
                    }
                }
            }
        }
    }
}

/* Stage 4: Inverse DCT */
void inverseDCTBlockComponent(int* component) {
    float intermediate[64];

    for (uint i = 0; i < 8; ++i) {
        const float g0 = component[0 * 8 + i] * s0;
        const float g1 = component[4 * 8 + i] * s4;
        const float g2 = component[2 * 8 + i] * s2;
        const float g3 = component[6 * 8 + i] * s6;
        const float g4 = component[5 * 8 + i] * s5;
        const float g5 = component[1 * 8 + i] * s1;
        const float g6 = component[7 * 8 + i] * s7;
        const float g7 = component[3 * 8 + i] * s3;

        const float f0 = g0;
        const float f1 = g1;
        const float f2 = g2;
        const float f3 = g3;
        const float f4 = g4 - g7;
        const float f5 = g5 + g6;
        const float f6 = g5 - g6;
        const float f7 = g4 + g7;

        const float e0 = f0;
        const float e1 = f1;
        const float e2 = f2 - f3;
        const float e3 = f2 + f3;
        const float e4 = f4;
        const float e5 = f5 - f7;
        const float e6 = f6;
        const float e7 = f5 + f7;
        const float e8 = f4 + f6;

        const float d0 = e0;
        const float d1 = e1;
        const float d2 = e2 * m1;
        const float d3 = e3;
        const float d4 = e4 * m2;
        const float d5 = e5 * m3;
        const float d6 = e6 * m4;
        const float d7 = e7;
        const float d8 = e8 * m5;

        const float c0 = d0 + d1;
        const float c1 = d0 - d1;
        const float c2 = d2 - d3;
        const float c3 = d3;
        const float c4 = d4 + d8;
        const float c5 = d5 + d7;
        const float c6 = d6 - d8;
        const float c7 = d7;
        const float c8 = c5 - c6;

        const float b0 = c0 + c3;
        const float b1 = c1 + c2;
        const float b2 = c1 - c2;
        const float b3 = c0 - c3;
        const float b4 = c4 - c8;
        const float b5 = c8;
        const float b6 = c6 - c7;
        const float b7 = c7;

        intermediate[0 * 8 + i] = b0 + b7;
        intermediate[1 * 8 + i] = b1 + b6;
        intermediate[2 * 8 + i] = b2 + b5;
        intermediate[3 * 8 + i] = b3 + b4;
        intermediate[4 * 8 + i] = b3 - b4;
        intermediate[5 * 8 + i] = b2 - b5;
        intermediate[6 * 8 + i] = b1 - b6;
        intermediate[7 * 8 + i] = b0 - b7;
    }
    for (uint i = 0; i < 8; ++i) {
        const float g0 = intermediate[i * 8 + 0] * s0;
        const float g1 = intermediate[i * 8 + 4] * s4;
        const float g2 = intermediate[i * 8 + 2] * s2;
        const float g3 = intermediate[i * 8 + 6] * s6;
        const float g4 = intermediate[i * 8 + 5] * s5;
        const float g5 = intermediate[i * 8 + 1] * s1;
        const float g6 = intermediate[i * 8 + 7] * s7;
        const float g7 = intermediate[i * 8 + 3] * s3;

        const float f0 = g0;
        const float f1 = g1;
        const float f2 = g2;
        const float f3 = g3;
        const float f4 = g4 - g7;
        const float f5 = g5 + g6;
        const float f6 = g5 - g6;
        const float f7 = g4 + g7;

        const float e0 = f0;
        const float e1 = f1;
        const float e2 = f2 - f3;
        const float e3 = f2 + f3;
        const float e4 = f4;
        const float e5 = f5 - f7;
        const float e6 = f6;
        const float e7 = f5 + f7;
        const float e8 = f4 + f6;

        const float d0 = e0;
        const float d1 = e1;
        const float d2 = e2 * m1;
        const float d3 = e3;
        const float d4 = e4 * m2;
        const float d5 = e5 * m3;
        const float d6 = e6 * m4;
        const float d7 = e7;
        const float d8 = e8 * m5;

        const float c0 = d0 + d1;
        const float c1 = d0 - d1;
        const float c2 = d2 - d3;
        const float c3 = d3;
        const float c4 = d4 + d8;
        const float c5 = d5 + d7;
        const float c6 = d6 - d8;
        const float c7 = d7;
        const float c8 = c5 - c6;

        const float b0 = c0 + c3;
        const float b1 = c1 + c2;
        const float b2 = c1 - c2;
        const float b3 = c0 - c3;
        const float b4 = c4 - c8;
        const float b5 = c8;
        const float b6 = c6 - c7;
        const float b7 = c7;

        component[i * 8 + 0] = b0 + b7 + 0.5f;
        component[i * 8 + 1] = b1 + b6 + 0.5f;
        component[i * 8 + 2] = b2 + b5 + 0.5f;
        component[i * 8 + 3] = b3 + b4 + 0.5f;
        component[i * 8 + 4] = b3 - b4 + 0.5f;
        component[i * 8 + 5] = b2 - b5 + 0.5f;
        component[i * 8 + 6] = b1 - b6 + 0.5f;
        component[i * 8 + 7] = b0 - b7 + 0.5f;
    }
}

void inverseDCT(const Header* image) {
    for (uint y = 0; y < image->blockHeight; y += image->verticalSamplingFactor) {
        for (uint x = 0; x < image->blockWidth; x += image->horizontalSamplingFactor) {
            for (uint i = 0; i < image->numComponents; ++i) {
                const ColorComponent* component = &image->colorComponents[i];
                for (uint v = 0; v < component->verticalSamplingFactor; ++v) {
                    for (uint h = 0; h < component->horizontalSamplingFactor; ++h) {
                        MCU* block = &image->blocks[(y + v) * image->blockWidthReal + (x + h)];
                        int* componentData;
                        switch (i) {
                            case 0:
                                componentData = block->y;
                                break;
                            case 1:
                                componentData = block->cb;
                                break;
                            case 2:
                                componentData = block->cr;
                                break;
                            default:
                                componentData = NULL;
                                break;
                        }
                        if (componentData) {
                            inverseDCTBlockComponent(componentData);
                        }
                    }
                }
            }
        }
    }
}

/* Stage 5: Upsampling */
void upsample(Header* image) {
    const uint vSamp = image->verticalSamplingFactor;
    const uint hSamp = image->horizontalSamplingFactor;

    printf("Upsampling Cb/Cr with factors %ux%u...\n", hSamp, vSamp);
        
    for (uint y = 0; y < image->blockHeight; y += vSamp) {
        for (uint x = 0; x < image->blockWidth; x += hSamp) {
            MCU* cbcrBlock = &image->blocks[y * image->blockWidthReal + x];
            
            /* Use countdown loops to match master branch (unsigned integer underflow pattern) */
            for (uint v = vSamp - 1; v < vSamp; --v) {
                for (uint h = hSamp - 1; h < hSamp; --h) {
                    MCU* yBlock = &image->blocks[(y + v) * image->blockWidthReal + (x + h)];
                    
                    /* Nearest-neighbor upsampling: replicate each Cb/Cr pixel */
                    /* Use countdown loops for py and px to match master branch exactly */
                    for (uint py = 7; py < 8; --py) {
                        for (uint px = 7; px < 8; --px) {
                            const uint pixel = py * 8 + px;
                            const uint cbcrPixelRow = py / vSamp + 4 * v;
                            const uint cbcrPixelColumn = px / hSamp + 4 * h;
                            const uint cbcrPixel = cbcrPixelRow * 8 + cbcrPixelColumn;
                            
                            yBlock->cb[pixel] = cbcrBlock->cb[cbcrPixel];
                            yBlock->cr[pixel] = cbcrBlock->cr[cbcrPixel];
                        }
                    }
                }
            }
        }
    }
}

/* Stage 6: Color Space Conversion */
void YCbCrToRGBBlock(MCU* block) {
    for (uint i = 0; i < 64; ++i) {
        int r = block->y[i]                                    + 1.402f * block->cr[i] + 128;
        int g = block->y[i] - 0.344f * block->cb[i] - 0.714f * block->cr[i] + 128;
        int b = block->y[i] + 1.772f * block->cb[i]                        + 128;
        
        if (r < 0)   r = 0;
        if (r > 255) r = 255;
        if (g < 0)   g = 0;
        if (g > 255) g = 255;
        if (b < 0)   b = 0;
        if (b > 255) b = 255;
        
        block->r[i] = r;
        block->g[i] = g;
        block->b[i] = b;
    }
}

void colorSpaceConversion(const Header* image) {
    for (uint y = 0; y < image->blockHeight; ++y) {
        for (uint x = 0; x < image->blockWidth; ++x) {
            MCU* block = &image->blocks[y * image->blockWidthReal + x];
            YCbCrToRGBBlock(block);
        }
    }
}

/* Helper functions for BMP writing */
void putInt(byte** bufferPos, const uint v) {
    *(*bufferPos)++ = v >>  0;
    *(*bufferPos)++ = v >>  8;
    *(*bufferPos)++ = v >> 16;
    *(*bufferPos)++ = v >> 24;
}

void putShort(byte** bufferPos, const uint v) {
    *(*bufferPos)++ = v >> 0;
    *(*bufferPos)++ = v >> 8;
}

/* Stage 7: Write BMP */
void writeBMP(const Header* image, const char* filename) {
    printf("Writing %s...\n", filename);
    
    FILE* outFile = fopen(filename, "wb");
    if (outFile == NULL) {
        printf("Error - Error opening output file\n");
        return;
    }

    const uint paddingSize = image->width % 4;
    const uint size = 14 + 12 + image->height * image->width * 3 + paddingSize * image->height;

    byte* buffer = (byte*)calloc(size, 1);
    if (buffer == NULL) {
        printf("Error - Memory error\n");
        fclose(outFile);
        return;
    }
    byte* bufferPos = buffer;

    *bufferPos++ = 'B';
    *bufferPos++ = 'M';
    putInt(&bufferPos, size);
    putInt(&bufferPos, 0);
    putInt(&bufferPos, 0x1A);
    putInt(&bufferPos, 12);
    putShort(&bufferPos, image->width);
    putShort(&bufferPos, image->height);
    putShort(&bufferPos, 1);
    putShort(&bufferPos, 24);

    for (uint y = image->height - 1; y < image->height; --y) {
        const uint blockRow = y / 8;
        const uint pixelRow = y % 8;
        for (uint x = 0; x < image->width; ++x) {
            const uint blockColumn = x / 8;
            const uint pixelColumn = x % 8;
            const MCU* block = &image->blocks[blockRow * image->blockWidthReal + blockColumn];
            const uint pixel = pixelRow * 8 + pixelColumn;
            *bufferPos++ = block->b[pixel];
            *bufferPos++ = block->g[pixel];
            *bufferPos++ = block->r[pixel];
        }
        for (uint i = 0; i < paddingSize; ++i) {
            *bufferPos++ = 0;
        }
    }

    fwrite(buffer, 1, size, outFile);
    fclose(outFile);
    free(buffer);
}

/* Cleanup */
void freeHeader(Header* image) {
    if (image != NULL) {
        if (image->blocks != NULL) {
            free(image->blocks);
        }
        if (image->filename != NULL) {
            free(image->filename);
        }
        bytearray_free(&image->huffmanData);
        free(image);
    }
}

/* Main function */
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Error - Invalid arguments\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        const char* filename = argv[i];
        
        struct timespec start, end;
        double stage_times[7] = {0};
        double total_time = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);

        /* Stage 1: Read JPEG */
        Header* image = readJPEG(filename);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[0] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        if (image == NULL) {
            continue;
        }
        if (image->blocks == NULL) {
            freeHeader(image);
            continue;
        }
        if (!image->valid) {
            freeHeader(image);
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 2: Huffman Decoding */
        huffmanDecoding(image);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[1] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[1] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        if (!image->valid) {
            freeHeader(image);
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 3: Dequantization */
        dequantize(image);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[2] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 4: Inverse DCT */
        inverseDCT(image);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[3] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 5: Upsampling */
        upsample(image);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[4] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 6: Color Space Conversion */
        colorSpaceConversion(image);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[5] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Stage 7: Output */
        const char* dot = strrchr(filename, '.');
        char* outFilename;
        if (dot == NULL) {
            outFilename = (char*)malloc(strlen(filename) + 20);
            sprintf(outFilename, "%s_decoder_c_out.bmp", filename);
        } else {
            size_t baseLen = dot - filename;
            outFilename = (char*)malloc(baseLen + 20);
            strncpy(outFilename, filename, baseLen);
            strcpy(outFilename + baseLen, "_decoder_c_out.bmp");
        }
        
        writeBMP(image, outFilename);
        clock_gettime(CLOCK_MONOTONIC, &end);
        stage_times[6] = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        free(outFilename);

        /* Calculate and print timing statistics */
        total_time = 0;
        for (int j = 0; j < 7; j++) {
            total_time += stage_times[j];
        }
        
        uint totalMCUs = image->blockHeightReal * image->blockWidthReal;
        printf("\nTotal MCUs processed: %u\n", totalMCUs);
        printf("\n=== Decoder Timing Statistics ===\n");
        printf("Stage 1 (Read JPEG):           %8.4f ms\n", stage_times[0] * 1000);
        printf("Stage 2 (Huffman Decoding):    %8.4f ms\n", stage_times[1] * 1000);
        printf("Stage 3 (Dequantization):      %8.4f ms\n", stage_times[2] * 1000);
        printf("Stage 4 (Inverse DCT):         %8.4f ms\n", stage_times[3] * 1000);
        printf("Stage 5 (Upsampling):          %8.4f ms\n", stage_times[4] * 1000);
        printf("Stage 6 (Color Conversion):    %8.4f ms\n", stage_times[5] * 1000);
        printf("Stage 7 (Write BMP):           %8.4f ms\n", stage_times[6] * 1000);
        printf("----------------------------------\n");
        printf("Total Time:                    %8.4f ms\n", total_time * 1000);
        printf("==================================\n\n");

        freeHeader(image);
    }
    
    return 0;
}
