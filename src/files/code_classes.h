#ifndef CODE_CLASSES_H
#define CODE_CLASSES_H

#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>


#define TOTAL_NUMBER_OF_CODE_CLASSES 4

uint8_t get_code_class_size(uint8_t code_class); 
uint8_t get_code_class_overhead(uint8_t code_class);
uint8_t get_header_overhead(uint8_t code_class, uint16_t seq_length);
uint16_t get_code_class_threshold(uint8_t code_class);

#endif