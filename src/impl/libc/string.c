#include "string.h"
#include <stdint.h>

void* memset(void* ptr, int value, size_t num) {
    uint8_t* p = ptr;
    for (size_t i = 0; i < num; i++) {
        p[i] = (uint8_t)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    uint8_t* d = dest;
    const uint8_t* s = src;
    for (size_t i = 0; i < num; i++) {
        d[i] = s[i];
    }
    return dest;
}
