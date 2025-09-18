#!/bin/bash

# Always work relative to the script's directory
#cd "$(dirname "$0")/src" || { echo "Directory src/ not found."; exit 1; }

output_file="combined_code.c"
rm -f "$output_file"
echo "// Combined C and H Files" > "$output_file"

process_files() {
    for path in "$@"; do
        if [[ -d "$path" ]]; then
            echo "Processing directory: $path"
            for file in "$path"/*; do
                if [[ -f "$file" && ( "$file" == *.c || "$file" == *.h ) ]]; then
                    echo -e "\n// === FILE: $file ===\n" >> "$output_file"
                    cat "$file" >> "$output_file"
                fi
            done
        elif [[ -f "$path" && ( "$path" == *.c || "$path" == *.h ) ]]; then
            echo "Processing file: $path"
            echo -e "\n// === FILE: $path ===\n" >> "$output_file"
            cat "$path" >> "$output_file"
        else
            echo "Warning: $path is not a valid .c/.h file or directory"
        fi
    done
}

# List actual files or folders correctly relative to src/

process_files "./bitmap.h" 
process_files "./bitmap.c" 
process_files "./sort.h" 
process_files "./sort.c" 
process_files "./rel_main.c"

echo "Combined all .h and .c files into $output_file"

