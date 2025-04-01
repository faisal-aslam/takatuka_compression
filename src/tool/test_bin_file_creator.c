#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Function to generate a random byte
unsigned char random_byte() {
    return (unsigned char)(rand() % 256);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <size_in_bytes>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int file_size = atoi(argv[2]);
    
    if (file_size <= 0) {
        printf("Invalid file size.\n");
        return 1;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    srand(time(NULL));

    int num_sequences;
    printf("How many binary sequences do you wish to enter? ");
    scanf("%d", &num_sequences);

    unsigned char *buffer = (unsigned char *)malloc(file_size);
    if (!buffer) {
        printf("Memory allocation failed.\n");
        fclose(file);
        return 1;
    }
    memset(buffer, 0, file_size);  // Initialize buffer with zeros

    int position = 0;

    for (int i = 0; i < num_sequences; i++) {
        int seq_length, occurrences;
        printf("Enter length of binary sequence %d (bytes): ", i + 1);
        scanf("%d", &seq_length);

        if (seq_length <= 0 || seq_length > file_size) {
            printf("Invalid sequence length.\n");
            continue;
        }

        unsigned char sequence[seq_length];
        printf("Enter binary sequence (in hex, space-separated): ");
        for (int j = 0; j < seq_length; j++) {
            int byte;
            scanf("%x", &byte);
            sequence[j] = (unsigned char)byte;
        }

        printf("Enter how many times it should appear: ");
        scanf("%d", &occurrences);

        for (int j = 0; j < occurrences; j++) {
            if (position + seq_length <= file_size) {
                memcpy(&buffer[position], sequence, seq_length);
                position += seq_length;
            } else {
                break;
            }
        }
    }

    // Fill the rest with random bytes
    while (position < file_size) {
        buffer[position++] = random_byte();
    }

    fwrite(buffer, 1, file_size, file);
    fclose(file);
    free(buffer);

    printf("Binary file '%s' created successfully with size %d bytes.\n", filename, file_size);
    return 0;
}
