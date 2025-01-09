/**
* TakaTuka Compression Algorithm © 2024 by Dr. Faisal ASlam is licensed under CC BY-NC-SA 4.0 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define BUFFER_SIZE 8192 // Define the size of the buffer for file operations

// Function to copy a single file from source to destination
void copy_file(const char *src, const char *dest) {
    // Open the source file in binary read mode
    FILE *source_file = fopen(src, "rb");
    if (!source_file) {
        perror("Failed to open source file"); // Print error if the file cannot be opened
        return;
    }

    // Open the destination file in binary write mode
    FILE *dest_file = fopen(dest, "wb");
    if (!dest_file) {
        perror("Failed to open destination file"); // Print error if the file cannot be created
        fclose(source_file);
        return;
    }

    char buffer[BUFFER_SIZE]; // Buffer to store file chunks
    size_t bytes_read;

    // Read from the source file and write to the destination file in chunks
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            perror("Failed to write to destination file"); // Print error if writing fails
            break;
        }
    }

    // Close the files after copying
    fclose(source_file);
    fclose(dest_file);
}

// Function to process a directory: copy all files and subdirectories
void process_directory(const char *src_dir, const char *dest_dir) {
    // Open the source directory
    DIR *dir = opendir(src_dir);
    if (!dir) {
        perror("Failed to open source directory"); // Print error if directory cannot be opened
        return;
    }

    struct dirent *entry; // Structure to store directory entry information

    // Loop through all entries in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip the special entries "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char src_path[PATH_MAX], dest_path[PATH_MAX];

        // Construct the full source and destination paths
        snprintf(src_path, PATH_MAX, "%s/%s", src_dir, entry->d_name);
        snprintf(dest_path, PATH_MAX, "%s/%s", dest_dir, entry->d_name);

        struct stat path_stat;

        // Get information about the source path
        if (stat(src_path, &path_stat) == -1) {
            perror("Failed to stat source path"); // Print error if stat fails
            continue;
        }

        // If the source is a regular file, copy it
        if (S_ISREG(path_stat.st_mode)) {
            copy_file(src_path, dest_path);
        }
        // If the source is a directory, create the destination directory and process it recursively
        else if (S_ISDIR(path_stat.st_mode)) {
            mkdir(dest_path, 0755); // Create the destination directory
            process_directory(src_path, dest_path); // Recursive call for subdirectory
        }
    }

    closedir(dir); // Close the directory after processing
}

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        return EXIT_FAILURE; // Exit with failure if arguments are incorrect
    }

    struct stat path_stat;

    // Get information about the source path
    if (stat(argv[1], &path_stat) == -1) {
        perror("Failed to stat source"); // Print error if stat fails
        return EXIT_FAILURE;
    }

    // If the source is a regular file, copy it
    if (S_ISREG(path_stat.st_mode)) {
        copy_file(argv[1], argv[2]);
    }
    // If the source is a directory, create the destination directory and process it
    else if (S_ISDIR(path_stat.st_mode)) {
        mkdir(argv[2], 0755); // Create the destination directory
        process_directory(argv[1], argv[2]); // Process the directory
    }
    // If the source is neither a file nor a directory, print an error
    else {
        fprintf(stderr, "Source is neither a regular file nor a directory\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS; // Exit successfully
}

