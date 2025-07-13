#include "group.h"
#include <math.h>

/*
 *    - 3-bit length
 *    - N bytes of sequence data
 *    - 2-bit group
 *    - groupCodeSize()
*/
uint8_t getHeaderOverhead(uint8_t group, uint16_t seq_length) {
	if (group == 0 || group == 1 || group ==2) {
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
        case 1: return 6;  // 6-bit codeword
        case 2: return 10; // 10-bit codeword       
        default:
            fprintf(stderr, "Invalid group %d Exiting!\n", group);            
            exit(0);
            return 0;
    }
}

uint16_t getGroupThreshold(uint8_t group) {
    switch (group) {
        case 0:
        case 1:
        case 2:
            return pow(2, groupCodeSize(group));
        //group 3 does not have any threshold
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
