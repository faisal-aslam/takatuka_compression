#include "common_types.h"
#include "hash_table.h"
#include "weighted_freq.h"

static inline int compareSequences(uint8_t *seq1, uint8_t *seq2, int length);

HashEntry *hashTable[HASH_TABLE_SIZE];

void initializeHashTable() {
    memset(hashTable, 0, sizeof(hashTable));
}

// Function to compare two binary sequences
static inline int compareSequences(uint8_t *seq1, uint8_t *seq2, int length) {
    if (seq1 == NULL || seq2 == NULL || length <= 0) {
        fprintf(stderr, "Error: Invalid sequence or length in compareSequences.\n");
        return -1;  // Indicate an error
    }
    return memcmp(seq1, seq2, length);
}

// Function to insert or update a sequence in the hash table
void updateHashTable(uint8_t *sequence, int length) {
    if (!sequence || length <= 0 || length < SEQ_LENGTH_START) {
        //fprintf(stderr, "Error: Invalid sequence or length in updateHashTable.\n");
        return;
    }

    unsigned int hashValue = fnv1a_hash(sequence, length);
    HashEntry *entry = hashTable[hashValue];

    // Traverse the linked list at the hash table index
    while (entry != NULL) {
        if (entry->seq->length == length && compareSequences(entry->seq->sequence, sequence, length) == 0) {
            // Sequence found, update count and frequency
            entry->seq->count++;
            entry->seq->frequency = entry->seq->length * entry->seq->count;
            return;
        }
        entry = entry->next;
    }

    // Sequence not found, create a new entry
    BinarySequence *newSeq = (BinarySequence *)malloc(sizeof(BinarySequence));
    if (!newSeq) {
        perror("Failed to allocate memory for BinarySequence");
        return;
    }

    newSeq->sequence = (uint8_t *)malloc(length * sizeof(uint8_t));
    if (!newSeq->sequence) {
        perror("Failed to allocate memory for sequence");
        free(newSeq);
        return;
    }

    memcpy(newSeq->sequence, sequence, length);
    newSeq->length = length;
    newSeq->count = 1;
    newSeq->frequency = length;
    newSeq->group = 0;

    // Create new hash table entry
    HashEntry *newEntry = (HashEntry *)malloc(sizeof(HashEntry));
    if (!newEntry) {
        perror("Failed to allocate memory for HashEntry");
        free(newSeq->sequence);
        free(newSeq);
        return;
    }

    newEntry->seq = newSeq;
    newEntry->next = hashTable[hashValue];
    hashTable[hashValue] = newEntry;
}


void cleanupHashTable() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = hashTable[i];
        while (entry != NULL) {
            HashEntry *next = entry->next;
            if (entry->seq) {
                free(entry->seq->sequence);
                free(entry->seq);
            }
            free(entry);
            entry = next;
        }
        hashTable[i] = NULL;
    }
}
