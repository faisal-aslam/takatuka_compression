#ifndef GROUP_H
#define GROUP_H

#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>


#define TOTAL_GROUPS 4

uint8_t groupCodeSize(uint8_t group); 
uint8_t groupOverHead(uint8_t group);
uint8_t getHeaderOverhead(uint8_t group, uint16_t seq_length);
uint16_t getGroupThreshold(uint8_t group);

#endif