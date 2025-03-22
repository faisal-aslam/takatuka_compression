#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define MAX_SEQ_LENGTH 64          // Maximum sequence length in bytes
#define HASH_TABLE_SIZE 1000003   // Prime number for hash table size
#define BLOCK_SIZE (1 << 20)      // Block size: 1 MB
#define SEQ_LENGTH_LIMIT 4  // Maximum sequence length to process (k)


// Structure to represent a binary sequence
typedef struct {
    uint8_t *sequence;  // Pointer to the binary sequence
    int length;         // Length of the sequence in bytes
    int count;          // Occurrence count
    int frequency;      // Weighted frequency (length * count)
} BinarySequence;

// Hash table entry
typedef struct HashEntry {
    BinarySequence *seq;
    struct HashEntry *next;
} HashEntry;

// Hash table
HashEntry *hashTable[HASH_TABLE_SIZE];

// Priority queue (min-heap) to store the most frequent sequences
BinarySequence *maxHeap[MAX_SEQ_LENGTH * 1000];  // Adjust size as needed
int heapSize = 0;

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

// Function to insert a sequence into the min-heap
void insertIntoMinHeap(BinarySequence *seq, int m) {
    if (heapSize < m) {
        maxHeap[heapSize] = seq;
        int i = heapSize;
        heapSize++;

        // Maintain the min-heap property
        while (i > 0 && maxHeap[(i - 1) / 2]->frequency > maxHeap[i]->frequency) {
            swap(i, (i - 1) / 2);
            i = (i - 1) / 2;
        }
    } else if (seq->frequency > maxHeap[0]->frequency) {
        // Replace the root with the new sequence
        maxHeap[0] = seq;
        minHeapify(0);
    }
}

// Function to extract the top m most frequent sequences
void extractTopSequences(int m, BinarySequence **result) {
    for (int i = 0; i < m && heapSize > 0; i++) {
        result[i] = maxHeap[0];  // Store the root (most frequent sequence)
        maxHeap[0] = maxHeap[heapSize - 1];  // Replace root with the last element
        heapSize--;
        minHeapify(0);  // Restore the min-heap property
    }
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
    if (block == NULL) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return;
    }

    uint8_t overlapBuffer[MAX_SEQ_LENGTH];  // Buffer to handle overlapping sequences
    int overlapSize = 0;

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) {
            break;  // End of file
        }

        processBlock(block, bytesRead, overlapBuffer, &overlapSize);
    }

    free(block);
    fclose(file);
}

// Function to build the min-heap from the hash table
void buildMinHeap(int m) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = hashTable[i];
        while (entry != NULL) {
            insertIntoMinHeap(entry->seq, m);
            entry = entry->next;
        }
    }
}

// Function to print the top m sequences
void printTopSequences(BinarySequence **topSequences, int m) {
    for (int i = 0; i < m; i++) {
        printf("%d: Sequence: ", i);
        for (int j = 0; j < topSequences[i]->length; j++) {
            printf("%02X ", topSequences[i]->sequence[j]);
        }
        printf("Frequency: %d\n", topSequences[i]->frequency);
    }
}

// Function to clean up allocated memory
void cleanup() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = hashTable[i];
        while (entry != NULL) {
            HashEntry *next = entry->next;
            free(entry->seq->sequence);  // Free the sequence
            free(entry->seq);           // Free the BinarySequence
            free(entry);                // Free the HashEntry
            entry = next;
        }
        hashTable[i] = NULL;  // Clear the hash table entry
    }
}

// Function to calculate m based on file size
int calculateM(long fileSize) {
    // Base case: 1 MB file -> m = 500
    if (fileSize <= (1 << 20)) {
        return 500;
    }
    // Scale m proportionally for larger files
    return (int)(500 * (fileSize / (double)(1 << 20)));
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
    int m = calculateM(fileSize);

    // Initialize hash table
    memset(hashTable, 0, sizeof(hashTable));

    // Process the file in blocks
    processFileInBlocks(filename, m);

    // Build the min-heap from the hash table
    buildMinHeap(m);

    // Extract the top m most frequent sequences
    BinarySequence **topSequences = (BinarySequence **)malloc(m * sizeof(BinarySequence *));
    extractTopSequences(m, topSequences);

    // Print the results
    printTopSequences(topSequences, m);

    // Clean up allocated memory
    cleanup();
    free(topSequences);

    return 0;
}
