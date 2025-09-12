#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Open input file
    FILE *input_file = fopen(argv[1], "rb");
    if (input_file == NULL) {
        perror("Error opening input file");
        return 1;
    }

    // Create output filename with .txt extension
    char output_filename[1024];
    snprintf(output_filename, sizeof(output_filename), "%s.txt", argv[1]);

    // Open output file
    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        perror("Error opening output file");
        fclose(input_file);
        return 1;
    }

    unsigned char byte;
    size_t bytes_read;
    int first_byte = 1;

    printf("Processing file: %s\n", argv[1]);
    printf("Output file: %s\n", output_filename);

    // Read file byte by byte
    while ((bytes_read = fread(&byte, 1, 1, input_file)) == 1) {
        if (!first_byte) {
            fprintf(output_file, " ");
        }
        fprintf(output_file, "%d", (int)byte);
        first_byte = 0;
    }

    // Check for read errors
    if (ferror(input_file)) {
        perror("Error reading input file");
        fclose(input_file);
        fclose(output_file);
        return 1;
    }

    // Close files
    fclose(input_file);
    fclose(output_file);

    printf("Conversion completed successfully!\n");
    return 0;
}
