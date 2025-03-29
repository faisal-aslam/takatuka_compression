#include "sequence_map.h"
#include "weighted_freq.h"

SequenceMapEntry *sequenceMap[HASH_TABLE_SIZE];

void initializeSequenceMap() {
    memset(sequenceMap, 0, sizeof(sequenceMap));
}

void freeSequenceMap() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (sequenceMap[i] != NULL) {
            free(sequenceMap[i]->key);
            free(sequenceMap[i]);
            sequenceMap[i] = NULL;
        }
    }
}

// Lookup a sequence in the map
BinarySequence* lookupSequence(uint8_t *sequence, int length) {
    unsigned int hashValue = fnv1a_hash(sequence, length);
    SequenceMapEntry *entry = sequenceMap[hashValue];
    
    // First check the exact slot
    if (entry != NULL && entry->key_length == length && 
        memcmp(entry->key, sequence, length) == 0) {
        return entry->sequence; // Found on first attempt
    }
    
    // Only do linear probing if there was a collision
    if (entry != NULL) {  // Collision occurred
        int attempts = 0;
        hashValue = (hashValue + 1) % HASH_TABLE_SIZE;
        
        while (attempts < HASH_TABLE_SIZE) {
            entry = sequenceMap[hashValue];
            
            if (entry == NULL) {
                return NULL; // Not found
            }
            
            if (entry->key_length == length && 
                memcmp(entry->key, sequence, length) == 0) {
                return entry->sequence; // Found during probing
            }
            
            // Move to next slot
            hashValue = (hashValue + 1) % HASH_TABLE_SIZE;
            attempts++;
        }
    }
    
    return NULL; // Not found
}

