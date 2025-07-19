//general_map.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>


static inline bool sequences_equal(const uint8_t *a, const uint8_t *b, uint8_t len) {
    switch (len) {
        case 1: return *a == *b;
        case 2: {
            uint16_t x, y;
            memcpy(&x, a, 2); memcpy(&y, b, 2); return x == y;
        }
        case 4: {
            uint32_t x, y;
            memcpy(&x, a, 4); memcpy(&y, b, 4); return x == y;
        }
        case 8: {
            uint64_t x, y;
            memcpy(&x, a, 8); memcpy(&y, b, 8); return x == y;
        }
        default: return memcmp(a, b, len) == 0;
    }

}

