#ifndef SEQUENCE_REPOSITORY_FREQ_H
#define SEQUENCE_REPOSITORY_FREQ_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

void seq_repo_init();
void seq_repo_cleanup();

uint32_t seq_repo_increase_frequency(const uint8_t* data, uint16_t length);
uint32_t seq_repo_decrease_frequency(const uint8_t* data, uint16_t length);
uint32_t seq_repo_get_frequency(const uint8_t* data, uint16_t length);

#endif
