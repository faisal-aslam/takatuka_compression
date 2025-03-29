// sequence_map.c
#include "sequence_map.h"
#include "weighted_freq.h"

SequenceMapEntry *sequenceMap[HASH_TABLE_SIZE] = {NULL};

void freeSequenceMap() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (sequenceMap[i] != NULL) {
            free(sequenceMap[i]->key);
            free(sequenceMap[i]);
            sequenceMap[i] = NULL;
        }
    }
}

BinarySequence* lookupSequence(uint8_t *sequence, int length) {
    if (!sequence || length <= 0) return NULL;
    
    unsigned int hashValue = fnv1a_hash(sequence, length);
    int attempts = 0;
    
    while (attempts < HASH_TABLE_SIZE) {
        SequenceMapEntry *entry = sequenceMap[hashValue];
        
        if (entry == NULL) {
            return NULL;
        }
        
        if (entry->key_length == length && 
            memcmp(entry->key, sequence, length) == 0) {
            return entry->sequence;
        }
        
        hashValue = (hashValue + 1) % HASH_TABLE_SIZE;
        attempts++;
    }
    
    return NULL;
}


