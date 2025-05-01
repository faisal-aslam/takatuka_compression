#include "group.h"

/*
 *    - 3-bit length
 *    - N bytes of sequence data
 *    - 2-bit group
 *    - groupCodeSize()
*/
uint8_t getHeaderOverhead(uint8_t group, uint16_t seq_length) {
	if (group < TOTAL_GROUPS) {
		return 3+(seq_length*8)+2+groupCodeSize(group);
	} else {		
        fprintf(stderr, "Invalid group %d Exiting!\n", group);
        exit(0);
        return 0;
    }
}

// Returns JUST the codeword bits (excluding flag + group bits)
uint8_t groupCodeSize(uint8_t group) {
    switch(group) {
        case 0: return 4;  // 4-bit codeword
        case 1: return 4;  // 4-bit codeword
        case 2: return 4;  // 4-bit codeword
        case 3: return 12; // 12-bit codeword
        default:
            fprintf(stderr, "Invalid group %d Exiting!\n", group);            
            exit(0);
            return 0;
    }
}

uint16_t getGroupThreshold(uint8_t group) {
    switch (group) {
        case 0:
            return 16;
        case 1:
            return 32;
        case 2: 
            return 48;
        case 3:
            return 4144;
        default:
            fprintf(stderr, "\n Illegal group of compression used \n");
            exit(EXIT_FAILURE);
    }
}


//Returns overhead of a group.
uint8_t groupOverHead(uint8_t group) {
	(void)group; // Suppressing unused parameter warning. In future we might need it.
	return 3;
}
