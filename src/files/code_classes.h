//code_class.h

#pragma once

#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_NUMBER_OF_CODE_CLASSES 4

uint8_t get_code_class_overhead(uint8_t code_class);

/* NOTE: add class2_bits - used when code_class == 2 to compute correct size */
uint8_t get_header_overhead(uint8_t code_class, uint16_t seq_length, uint8_t class2_bits);

uint16_t get_code_class_threshold(uint8_t code_class, uint8_t class2_bits);
uint8_t get_code_class_size(uint8_t code_class, uint8_t class2_bits);

uint8_t calculate_class2_bits(uint16_t class2_codes);
