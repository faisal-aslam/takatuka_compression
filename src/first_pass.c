#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "xxhash.h"

#define BLOCK_SIZE 65536
#define MIN_SEQ_LEN 2
#define MAX_SEQ_LEN 256
#define NUMBER_OF_SEQUENCES 1000
#define HASH_MAP_SIZE 1000003  // Large prime

typedef struct SeqEntry {
    uint8_t sequence[MAX_SEQ_LEN];
    uint8_t length;
    uint32_t frequency;
    struct SeqEntry* next;
} SeqEntry;

static SeqEntry* hash_map[HASH_MAP_SIZE] = {0};

// Use XXH3 for fast hashing of binary sequences
static inline uint32_t hash_sequence(const uint8_t* seq, uint8_t len) {
    return (uint32_t)(XXH3_64bits(seq, len) % HASH_MAP_SIZE);
}

void add_sequence(const uint8_t* seq, uint8_t len) {
    uint32_t hash = hash_sequence(seq, len);
    SeqEntry* entry = hash_map[hash];

    while (entry) {
        if (entry->length == len && memcmp(entry->sequence, seq, len) == 0) {
            entry->frequency++;
            return;
        }
        entry = entry->next;
    }

    SeqEntry* new_entry = malloc(sizeof(SeqEntry));
    if (!new_entry) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    memcpy(new_entry->sequence, seq, len);
    new_entry->length = len;
    new_entry->frequency = 1;
    new_entry->next = hash_map[hash];
    hash_map[hash] = new_entry;
}

void process_file(FILE* fp) {
    uint8_t buffer[BLOCK_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            for (uint8_t len = MIN_SEQ_LEN; len <= MAX_SEQ_LEN; len++) {
                if (i + len <= bytes_read) {
                    add_sequence(&buffer[i], len);
                }
            }
        }
    }
}

int compare_entries(const void* a, const void* b) {
    const SeqEntry* ea = *(const SeqEntry**)a;
    const SeqEntry* eb = *(const SeqEntry**)b;
    return (int)eb->frequency - (int)ea->frequency;
}

void extract_top_sequences(void) {
    SeqEntry** all = malloc(HASH_MAP_SIZE * sizeof(SeqEntry*));
    if (!all) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    size_t count = 0;
    for (size_t i = 0; i < HASH_MAP_SIZE; i++) {
        SeqEntry* entry = hash_map[i];
        while (entry) {
            all[count++] = entry;
            entry = entry->next;
        }
    }

    qsort(all, count, sizeof(SeqEntry*), compare_entries);

    size_t top = count < NUMBER_OF_SEQUENCES ? count : NUMBER_OF_SEQUENCES;
    printf("Top %zu sequences:\n", top);

    for (size_t i = 0; i < top; i++) {
        for (uint8_t j = 0; j < all[i]->length; j++) {
            printf("%02X ", all[i]->sequence[j]);
        }
        printf("- %u\n", all[i]->frequency);
    }

    free(all);
}

void free_map(void) {
    for (size_t i = 0; i < HASH_MAP_SIZE; i++) {
        SeqEntry* entry = hash_map[i];
        while (entry) {
            SeqEntry* next = entry->next;
            free(entry);
            entry = next;
        }
        hash_map[i] = NULL;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    process_file(fp);
    fclose(fp);

    extract_top_sequences();
    free_map();
    return 0;
}
