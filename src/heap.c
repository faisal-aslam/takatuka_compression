// heap.c
#include "common_types.h"
#include "heap.h"
#include "weighted_freq.h"
#include "hash_table.h" 

static void assignGroupsByFrequency();

BinarySequence *maxHeap[SEQ_LENGTH_LIMIT * MAX_NUMBER_OF_SEQUENCES];
int heapSize = 0;

// Function to swap two elements in the heap
static inline void swap(int i, int j) {
    BinarySequence *temp = maxHeap[i];
    maxHeap[i] = maxHeap[j];
    maxHeap[j] = temp;
}

// Function to maintain the min-heap property
static inline void minHeapify(int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < heapSize && maxHeap[left]->frequency < maxHeap[smallest]->frequency) {
        smallest = left;
    }
    if (right < heapSize && maxHeap[right]->frequency < maxHeap[smallest]->frequency) {
        smallest = right;
    }
    if (smallest != i) {
        swap(i, smallest);
        minHeapify(smallest);
    }
}


/* Inserts a sequence into the min-heap while maintaining heap property
 * Parameters:
 *   seq - sequence to consider (does NOT take ownership)
 */
void insertIntoMinHeap(BinarySequence *seq) {
    if (seq == NULL || seq->length < SEQ_LENGTH_START) {
         return;
    }
    BinarySequence *heapSeq = calloc(1, sizeof(BinarySequence));
    if (!heapSeq) return;

    heapSeq->sequence = malloc(seq->length);
    if (!heapSeq->sequence) {
        free(heapSeq);
        return;
    }
    memcpy(heapSeq->sequence, seq->sequence, seq->length);
    heapSeq->length = seq->length;
    heapSeq->count = seq->count;
    heapSeq->frequency = seq->frequency;
    heapSeq->group = seq->group;

    if (heapSize < MAX_NUMBER_OF_SEQUENCES) {
        maxHeap[heapSize] = heapSeq;
        int i = heapSize++;
        while (i > 0 && maxHeap[(i - 1) / 2]->frequency > maxHeap[i]->frequency) {
            swap(i, (i - 1) / 2);
            i = (i - 1) / 2;
        }
    } else if (heapSeq->frequency > maxHeap[0]->frequency) {
        // Free old root before replacing
        free(maxHeap[0]->sequence);
        free(maxHeap[0]); // Add this line

        maxHeap[0] = heapSeq;
        minHeapify(0);
    } else {
        // Not inserted, so free it
        free(heapSeq->sequence);
        free(heapSeq);
    }
}

/* Builds a min-heap of the most frequent sequences from the hash table
 */
void buildMinHeap() {
    // Free any pre-existing heap memory before rebuilding
    while (heapSize > 0) {
        free(maxHeap[--heapSize]->sequence);
        free(maxHeap[heapSize]);
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = hashTable[i];
        while (entry != NULL) {
            insertIntoMinHeap(entry->seq);
            entry = entry->next;
        }
    }
    assignGroupsByFrequency();
}

/* Extracts the top MAX_NUMBER_OF_SEQUENCES sequences that meet the minimum savings requirement
 * Parameters:
 *   result - output array for the extracted sequences (caller must free)
 */
void extractTopSequences(BinarySequence **result) {
    int count = 0;

    while (heapSize > 0 && count < MAX_NUMBER_OF_SEQUENCES) {
        BinarySequence *seq = maxHeap[0];
        maxHeap[0] = maxHeap[--heapSize];
        minHeapify(0);
        long potential_savings = (seq->length * 8 - (groupCodeSize(seq->group)+groupOverHead(seq->group))) * seq->count;
        //printf(" \n\n potential_savings=%ld, seq->group (%d) <= TOTAL_GROUPS (%d)", potential_savings, seq->group, TOTAL_GROUPS);
        if (potential_savings >= LEAST_REDUCTION && seq->group <= TOTAL_GROUPS) {
            result[count] = seq;
            
            unsigned int hashValue = fnv1a_hash(seq->sequence, seq->length);
            int attempts = 0;
            
            while (attempts < HASH_TABLE_SIZE) {
                if (sequenceMap[hashValue] == NULL) {
                    SequenceMapEntry *entry = malloc(sizeof(SequenceMapEntry));
                    if (!entry) break;
                    
                    entry->key = malloc(seq->length);
                    if (!entry->key) {
                        free(entry);
                        break;
                    }
                    
                    memcpy(entry->key, seq->sequence, seq->length);
                    entry->key_length = seq->length;
                    entry->sequence = seq;
                    
                    sequenceMap[hashValue] = entry;
                    count++;
                    break;
                }
                
                hashValue = (hashValue + 1) % HASH_TABLE_SIZE;
                attempts++;
            }
            
            if (attempts >= HASH_TABLE_SIZE) {
                fprintf(stderr, "Error: Hash table full, couldn't store sequence\n");
                free(seq->sequence);
                free(seq);
            }
        } else {
            free(seq->sequence);
            free(seq);
        }
    }
	printf("\n Sequences found = %d\n", count);
    while (count < MAX_NUMBER_OF_SEQUENCES) {
        result[count++] = NULL;
    }
}
void cleanupHeap() {
    while (heapSize > 0) {
        heapSize--;
        if (maxHeap[heapSize]) {
            free(maxHeap[heapSize]->sequence);
            free(maxHeap[heapSize]);
            maxHeap[heapSize] = NULL;
        }
    }
}

static inline int assignGroupHelper(int *elementsInGroup, int group, int current_index) {
    int code_size = groupCodeSize(group) + groupOverHead(group);
    long potential_savings = (maxHeap[current_index]->length * 8 - code_size) * maxHeap[current_index]->count;
    
    if (potential_savings > LEAST_REDUCTION) {
        maxHeap[current_index]->group = group;
        (*elementsInGroup)++;
        return 1;
    }
    return 0;
}

static void assignGroupsByFrequency() {
    BinarySequence **validSeqs = malloc(heapSize * sizeof(BinarySequence *));
    int validCount = 0;
    int elementsInGroup = 0;
    
    for (int i = 0; i < heapSize; i++) {
        uint8_t assigned_group = 0;
        
        if (elementsInGroup < GROUP1_THRESHOLD) {
            assigned_group = assignGroupHelper(&elementsInGroup, 0, i);
        } 
        else if (elementsInGroup < GROUP2_THRESHOLD) {
            assigned_group = assignGroupHelper(&elementsInGroup, 1, i);
        } 
        else if (elementsInGroup < GROUP3_THRESHOLD) {
            assigned_group = assignGroupHelper(&elementsInGroup, 2, i);
        } 
        else if (elementsInGroup < GROUP4_THRESHOLD) {
            assigned_group = assignGroupHelper(&elementsInGroup, 3, i);
        }
        
        if (assigned_group) {
            validSeqs[validCount++] = maxHeap[i];
        } else {
            free(maxHeap[i]->sequence);
            free(maxHeap[i]);
        }
    }
    
    memcpy(maxHeap, validSeqs, validCount * sizeof(BinarySequence *));
    heapSize = validCount;
    free(validSeqs);
}

