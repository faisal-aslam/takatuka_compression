#include "code_classes.h"
#include <math.h>

/*
 *    - 3-bit length
 *    - N bytes of sequence data
 *    - 2-bit code_class
 *    - code_classCodeSize()
*/
uint8_t get_header_overhead(uint8_t code_class, uint16_t seq_length) {
	if (code_class == 0 || code_class == 1 || code_class ==2) {
		return 3+(seq_length*8)+2+get_code_class_size(code_class);
	} else {		
        fprintf(stderr, "Invalid code_class %d Exiting!\n", code_class);
        exit(0);
        return 0;
    }
}

// Returns JUST the codeword bits (excluding flag + code_class bits)
uint8_t get_code_class_size(uint8_t code_class) {
    switch(code_class) {
        case 0: return 4;  // 4-bit codeword 
        case 1: return 6;  // 6-bit codeword
        case 2: return 10; // 10-bit codeword       
        default:
            fprintf(stderr, "Invalid code_class %d Exiting!\n", code_class);            
            exit(0);
            return 0;
    }
}

uint16_t get_code_class_threshold(uint8_t code_class) {
    switch (code_class) {
        case 0:
        case 1:
        case 2:
            return pow(2, get_code_class_size(code_class));
        //code_class 3 does not have any threshold
        default:
            fprintf(stderr, "\n Illegal code_class of compression used \n");
            exit(EXIT_FAILURE);
    }
}


//Returns overhead of a code_class.
uint8_t get_code_class_overhead(uint8_t code_class) {
	(void)code_class; // Suppressing unused parameter warning. In future we might need it.
	return 3;
}
