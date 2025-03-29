// heap.c
#include "common_types.h"
#include "heap.h"
#include "weighted_freq.h"
#include "hash_table.h" 


BinarySequence *maxHeap[SEQ_LENGTH_LIMIT * MAX_NUMBER_OF_SEQUENCES];
int heapSize = 0;

// Function to swap two elements in the heap
void swap(int i, int j) {
    BinarySequence *temp = maxHeap[i];
    maxHeap[i] = maxHeap[j];
    maxHeap[j] = temp;
}

// Function to maintain the min-heap property
void minHeapify(int i) {
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
 *   m - maximum heap size
 */
void insertIntoMinHeap(BinarySequence *seq, int m) {
    if (seq == NULL || seq->length < SEQ_LENGTH_START) {
         return;
    }
    BinarySequence *heapSeq = malloc(sizeof(BinarySequence));
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
    heapSeq->potential_savings = seq->potential_savings;
    heapSeq->group = seq->group;

    if (heapSize < m) {
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
 * Parameters:
 *   m - maximum number of sequences to keep in the heap
 */
void buildMinHeap(int m) {
    // Free any pre-existing heap memory before rebuilding
    while (heapSize > 0) {
        free(maxHeap[--heapSize]->sequence);
        free(maxHeap[heapSize]);
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = hashTable[i];
        while (entry != NULL) {
            insertIntoMinHeap(entry->seq, m);
            entry = entry->next;
        }
    }
    assignGroupsByFrequency();
}

/* Extracts the top m sequences that meet the minimum savings requirement
 * Parameters:
 *   m - number of sequences to extract
 *   result - output array for the extracted sequences (caller must free)
 */
void extractTopSequences(int m, BinarySequence **result) {
    int count = 0;

    while (heapSize > 0 && count < m) {
        BinarySequence *seq = maxHeap[0];
        maxHeap[0] = maxHeap[--heapSize];
        minHeapify(0);
        
        if (seq->potential_savings >= LEAST_REDUCTION && seq->group <= 4) {
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

    while (count < m) {
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

/* Assigns groups by sorting only the top m sequences */
void assignGroupsByFrequency() {
    // Temporary array for sorting
    BinarySequence **temp = malloc(heapSize * sizeof(BinarySequence *));
    memcpy(temp, maxHeap, heapSize * sizeof(BinarySequence *));
    
    // Sort just the top sequences
    qsort(temp, heapSize, sizeof(BinarySequence *), compareByFrequency);
    
    int elementsInGroup = 0;
    
    for (int i = 0; i < heapSize; i++) {
        long potential_savings = 0;
        
        if (elementsInGroup < GROUP1_THRESHOLD) {
            potential_savings = (temp[i]->length * 8 - GROUP1_CODE_SIZE) * temp[i]->count;
            //printf("potential_savings=%ld \n", potential_savings);  
            if (potential_savings > LEAST_REDUCTION) {  // Only assign if savings is larger than LEAST_REDUCTION.
                temp[i]->group = 1;
                temp[i]->potential_savings = potential_savings;
                elementsInGroup++;
            } else {
                temp[i]->group = 10;
                temp[i]->potential_savings = potential_savings;
            }
        }
        else if (elementsInGroup < GROUP2_THRESHOLD) {
            potential_savings = (temp[i]->length * 8 - GROUP2_CODE_SIZE) * temp[i]->count;
            if (potential_savings > LEAST_REDUCTION) { // Only assign if savings is larger than LEAST_REDUCTION.
                temp[i]->group = 2;
                temp[i]->potential_savings = potential_savings;
                elementsInGroup++;
            } else {
                temp[i]->group = 20;
                temp[i]->potential_savings = potential_savings;
            }
        }
        else if (elementsInGroup < GROUP3_THRESHOLD) {
            potential_savings = (temp[i]->length * 8 - GROUP3_CODE_SIZE) * temp[i]->count;
            if (potential_savings > LEAST_REDUCTION) { // Only assign if savings is larger than LEAST_REDUCTION.
                temp[i]->group = 3;
                temp[i]->potential_savings = potential_savings;
                elementsInGroup++;
            } else {
                temp[i]->group = 30;
                temp[i]->potential_savings = potential_savings;
            }
 
        }
        else if (elementsInGroup < GROUP4_THRESHOLD) {
            potential_savings = (temp[i]->length * 8 - GROUP4_CODE_SIZE) * temp[i]->count;
            if (potential_savings > LEAST_REDUCTION) {
                temp[i]->group = 4;
                temp[i]->potential_savings = potential_savings;
                elementsInGroup++;
            } else {
                temp[i]->group = 40;
                temp[i]->potential_savings = potential_savings;
            }
       }
    }
    
    free(temp);
}

/* Compare function for sorting by weighted frequency (descending) */
int compareByFrequency(const void *a, const void *b) {
    const BinarySequence *seqA = *(const BinarySequence **)a;
    const BinarySequence *seqB = *(const BinarySequence **)b;
    return seqB->frequency - seqA->frequency;  // Descending order
}
