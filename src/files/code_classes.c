//code_class.c

// src/files/code_classes.c

#include "code_classes.h"

/*
 * Header overhead layout per sequence entry (in bits):
 *  - 3 bits : (reserved for something in your original format — keep it)
 *  - N bytes * 8 : sequence bytes
 *  - 2 bits : code_class prefix
 *  - M bits : code index (depends on class: class0/class1 fixed, class2 dynamic)
 *
 * get_header_overhead returns this total in BITS or BYTES? Your old code
 * returned an integer that was used only for relative calculations; keep the same
 * semantics (here returning number of bits).
 */
uint8_t get_header_overhead(uint8_t code_class, uint16_t seq_length, uint8_t class2_bits) {
    if (code_class == 0 || code_class == 1 || code_class == 2) {
        /* 3 bits + seq_length*8 + 2 bits class + code bits for that class */
        uint8_t code_bits = get_code_class_size(code_class, class2_bits);
        return (uint8_t)(3 + (seq_length * 8) + 2 + code_bits);
    } else {
        fprintf(stderr, "Invalid code_class %d Exiting (get_header_overhead)!\n", code_class);
        exit(EXIT_FAILURE);
        return 0;
    }
}

/*
 * calculate_class2_bits:
 *  - returns smallest n such that (1u << n) >= class2_codes
 *  - returns 0 for class2_codes == 0
 *  - returns 0 for class2_codes == 1 (i.e. 1 entry requires 0 index bits)
 *    If you prefer a minimum of 1 bit for any non-zero count, change the return
 *    to `return (n == 0) ? 1 : n;`.
 */
uint8_t calculate_class2_bits(uint16_t class2_codes) {
    if (class2_codes == 0) return 0;

    uint8_t n = 0;
    while ((n < 31) && ((1u << n) < (uint32_t)class2_codes)) {
        n++;
    }
    return n;
}

/* Return the number of bits used for the index portion (excluding the 2-bit class prefix) */
uint8_t get_code_class_size(uint8_t code_class, uint8_t class2_bits) {
    switch (code_class) {
        case 0: return 4;              /* fixed */
        case 1: return 5;              /* fixed — note you had 5 in your last edit */
        case 2: return class2_bits;    /* dynamic */
        default:
            fprintf(stderr, "Invalid code_class %d Exiting (get_code_class_size)!\n", code_class);
            exit(EXIT_FAILURE);
            return 0;
    }
}

/* Return threshold (capacity) for given class as uint16_t. For class2 uses class2_bits. */
uint16_t get_code_class_threshold(uint8_t code_class, uint8_t class2_bits) {
    uint8_t bits;
    if (code_class == 2) {
        bits = class2_bits;
    } else {
        bits = get_code_class_size(code_class, 0); /* class2_bits unused for non-class2 */
    }

    if (bits >= 16) {
        /* uint16_t return can't represent >2^15 safely here. If you expect >65535
           entries for a class2, change return type to uint32_t. */
        fprintf(stderr, "Requested threshold bits too large: %u\n", bits);
        exit(EXIT_FAILURE);
    }
    return (uint16_t)(1u << bits);
}

/* Returns overhead of a code_class (kept same as before) */
uint8_t get_code_class_overhead(uint8_t code_class) {
    (void)code_class; // unused for now
    return 3;
}
