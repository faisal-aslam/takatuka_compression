#define _POSIX_C_SOURCE 199309L
#include "timer.h"
#include <time.h>

static struct timespec start_time;

void init_timer() {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

uint64_t get_elapsed_ms() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    uint64_t start_ms = (uint64_t)start_time.tv_sec * 1000 + start_time.tv_nsec / 1000000;
    uint64_t current_ms = (uint64_t)current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;

    return current_ms - start_ms;
}
