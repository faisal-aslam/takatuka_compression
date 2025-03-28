#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#define SEQ_LENGTH_LIMIT 4
#define HASH_TABLE_SIZE 1000003
#define BLOCK_SIZE (1 << 20)

#define CODE_PER_MEGA_BYTE 500
#define LEAST_REDUCTION 8  // Minimum bits we need to save to consider compression

// Group definitions
#define GROUP1_THRESHOLD 16    // Top 16 sequences
#define GROUP2_THRESHOLD 32    // Next 32 sequences (16+16)
#define GROUP3_THRESHOLD 48   // Next 256 sequences (32+16)
#define GROUP4_THRESHOLD 4144  // Next 4096 sequences (48+2048)
#define MAX_NUMBER_OF_SEQUENCES 4144

// Codeword sizes including overhead (prefix + codeword)
#define GROUP1_CODE_SIZE 7   // 3 + 4 bits
#define GROUP2_CODE_SIZE 7   // 3 + 4 bits
#define GROUP3_CODE_SIZE 7   // 3 + 4 bits
#define GROUP4_CODE_SIZE 15  // 3 + 12 bits

// Structure to represent a binary sequence
typedef struct {
    uint8_t *sequence;
    int length;         // Length in bytes
    int count;          // Occurrence count
    int frequency;      // length * count
    long potential_savings;  // (original_bits - code_size) * count
    int group;          // 1-4 based on frequency ranking
} BinarySequence;

// Hash table entry
typedef struct HashEntry {
    BinarySequence *seq;
    struct HashEntry *next;
} HashEntry;

// Hash table
HashEntry *hashTable[HASH_TABLE_SIZE];

// Priority queue (min-heap) to store the most frequent sequences
BinarySequence *maxHeap[SEQ_LENGTH_LIMIT * MAX_NUMBER_OF_SEQUENCES];  // Adjust size as needed
int heapSize = 0;

// Function declarations
unsigned int fnv1a_hash(uint8_t *sequence, int length);
int compareSequences(uint8_t *seq1, uint8_t *seq2, int length);
void updateHashTable(uint8_t *sequence, int length);
void swap(int i, int j);
void minHeapify(int i);
void insertIntoMinHeap(BinarySequence *seq, int m);
void extractTopSequences(int m, BinarySequence **result);
void processBlock(uint8_t *block, long blockSize, uint8_t *overlapBuffer, int *overlapSize);
void processFileInBlocks(const char *filename, int m);
void buildMinHeap(int m);
void printTopSequences(BinarySequence **topSequences, int m);
void cleanup();
int calculateM(long fileSize);
void assignGroupsByFrequency(int m);
int compareByFrequency(const void *a, const void *b);

// Function to compute a hash value for a binary sequence using FNV-1a
unsigned int fnv1a_hash(uint8_t *sequence, int length) {
    const unsigned int FNV_prime = 16777619;
    unsigned int hash = 2166136261;  // FNV offset basis

    for (int i = 0; i < length; i++) {
        hash ^= sequence[i];
        hash *= FNV_prime;
    }

    return hash % HASH_TABLE_SIZE;
}

// Function to compare two binary sequences
int compareSequences(uint8_t *seq1, uint8_t *seq2, int length) {
    if (seq1 == NULL || seq2 == NULL || length <= 0) {
        fprintf(stderr, "Error: Invalid sequence or length in compareSequences.\n");
        return -1;  // Indicate an error
    }
    return memcmp(seq1, seq2, length);
}

// Function to insert or update a sequence in the hash table
void updateHashTable(uint8_t *sequence, int length) {
    if (!sequence || length <= 0) {
        fprintf(stderr, "Error: Invalid sequence or length in updateHashTable.\n");
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
    newSeq->potential_savings = 0;
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



/* Compare function for sorting by weighted frequency (descending) */
int compareByFrequency(const void *a, const void *b) {
    const BinarySequence *seqA = *(const BinarySequence **)a;
    const BinarySequence *seqB = *(const BinarySequence **)b;
    return seqB->frequency - seqA->frequency;  // Descending order
}

/* Assigns groups by sorting only the top m sequences */
void assignGroupsByFrequency(int m) {
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


// Function to process a block of data
void processBlock(uint8_t *block, long blockSize, uint8_t *overlapBuffer, int *overlapSize) {
    // Combine the overlap buffer with the current block
    uint8_t *combinedBuffer = (uint8_t *)malloc(*overlapSize + blockSize);
    if (combinedBuffer == NULL) {
        perror("Failed to allocate memory for combinedBuffer");
        return;
    }

    memcpy(combinedBuffer, overlapBuffer, *overlapSize);
    memcpy(combinedBuffer + *overlapSize, block, blockSize);
    long combinedSize = *overlapSize + blockSize;

    // Count sequences of lengths 1 to 4 in the combined buffer
    for (int len = 1; len <= SEQ_LENGTH_LIMIT; len++) {
        for (long i = 0; i <= combinedSize - len; i++) {
            updateHashTable(combinedBuffer + i, len);
        }
    }
    
    // Update the overlap buffer for the next block
    *overlapSize = (SEQ_LENGTH_LIMIT - 1 < combinedSize) ? SEQ_LENGTH_LIMIT - 1 : combinedSize;
    memcpy(overlapBuffer, combinedBuffer + combinedSize - *overlapSize, *overlapSize);

    free(combinedBuffer);
}

// Function to read and process the file in blocks
void processFileInBlocks(const char *filename, int m) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    uint8_t *block = (uint8_t *)malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return;
    }

    // Local overlap buffer (only needs SEQ_LENGTH_LIMIT-1 bytes)
    uint8_t overlapBuffer[SEQ_LENGTH_LIMIT - 1] = {0};
    int overlapSize = 0;

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        // Process current block with overlap
        processBlock(block, bytesRead, overlapBuffer, &overlapSize);
    }

    free(block);
    fclose(file);
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
    assignGroupsByFrequency(m);
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
        result[count++] = seq;
    }
 /*
 if (seq->potential_savings >= LEAST_REDUCTION) {
            result[count++] = seq;
     } else {
            // Free sequences that don't meet the threshold
            free(seq->sequence);
            free(seq);
}
 */
    // Fill remaining slots with NULL
    while (count < m) {
        result[count++] = NULL;
    }
}


// Simplified print function - now just displays all sequences in results
void printTopSequences(BinarySequence **topSequences, int m) {
    long total_savings = 0;
    for (int i = 0; i < m; i++) {
        if (topSequences[i] == NULL) {
            //printf("%d: [EMPTY SLOT]\n", i);
            continue;
        }
        
        printf("%d: Sequence: ", i);
        for (int j = 0; j < topSequences[i]->length; j++) {
            printf("%02X ", topSequences[i]->sequence[j]);
        }
        printf("Count: %d, Freq: %d, Group: %d, Savings: %ld bits\n",
               topSequences[i]->count,
               topSequences[i]->frequency,
               topSequences[i]->group,
               topSequences[i]->potential_savings);
       if (topSequences[i]->group<=4) {
	       total_savings += topSequences[i]->potential_savings;
	   }
    }
    printf("Total Savings =%ld", total_savings);
}

// Function to clean up allocated memory
void cleanup() {
    // Free hash table memory
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

    // Free min-heap sequences
    for (int i = 0; i < heapSize; i++) {
        if (maxHeap[i]) {
            free(maxHeap[i]->sequence);
            free(maxHeap[i]);
        }
    }
    heapSize = 0;
}




// Function to calculate m (i.e. number of codewords) based on file size
int calculateM(long fileSize) {
    // Base case: 1 MB file -> m = CODE_PER_MEGA_BYTE
    if (fileSize <= (1 << 20)) {
        return CODE_PER_MEGA_BYTE;
    }
    // Scale m proportionally for larger files
    // We do not allow more than MAX_NUMBER_OF_SEQUENCES at the moment.
    int ret = (int)(CODE_PER_MEGA_BYTE * (fileSize / (double)(1 << 20)));
    if (ret > MAX_NUMBER_OF_SEQUENCES) {
        return MAX_NUMBER_OF_SEQUENCES;
    }
    return ret;
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 2) {  // Now only <binary_file> is required
        printf("Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    // Open the file to determine its size
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    // Calculate m based on file size
    int m = MAX_NUMBER_OF_SEQUENCES;//calculateM(fileSize);

    // Initialize hash table
    memset(hashTable, 0, sizeof(hashTable));

    // Process the file in blocks
    processFileInBlocks(filename, m);

    // Build the min-heap from the hash table
    buildMinHeap(m);

    // Extract the top m most frequent sequences
    BinarySequence **topSequences = (BinarySequence **)malloc(m * sizeof(BinarySequence *));
    if (!topSequences) {
        perror("Failed to allocate memory for topSequences");
        cleanup();
        return 1;
    }
    extractTopSequences(m, topSequences);

    // Print the results
    printTopSequences(topSequences, m);

    // Clean up allocated memory
    for (int i = 0; i < m; i++) {
        if (topSequences[i]) {
            free(topSequences[i]->sequence);
            free(topSequences[i]);
        }
    }
    free(topSequences);
    cleanup();

    return 0;
}

