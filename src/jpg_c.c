#include "jpg_c.h"
#include <stdio.h>
#include <stdlib.h>

/* ByteArray implementation - common for both encoder and decoder */
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
