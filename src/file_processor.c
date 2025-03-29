#include "file_processor.h"
#include "weighted_freq.h"

/* Function to read and process the file in blocks
 * Parameters:
 *   filename - name of the file to encrypt
 *   m - maximum number of sequences to encode
 */
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
    for (int len = SEQ_LENGTH_START; len <= SEQ_LENGTH_LIMIT; len++) {
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

