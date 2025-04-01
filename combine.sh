#!/bin/bash
cd src/
rm -f combined_code.c
output_file="combined_code.c"

echo "// Combined C and H Files" > "$output_file"

# Function to process files with full path comments
process_files() {
    local dir=$1
    for file in "$dir"/*; do
        if [[ -f "$file" ]]; then
            case "$file" in
                *.h)
                    echo -e "\n// === FILE: $file ===\n" >> "$output_file"
                    cat "$file" >> "$output_file"
                    ;;
                *.c)
                    echo -e "\n// === FILE: $file ===\n" >> "$output_file"
                    cat "$file" >> "$output_file"
                    ;;
            esac
        fi
    done
}

# Process main src files
process_files "."

# Process second_pass files if directory exists
if [[ -d "second_pass" ]]; then
    process_files "second_pass"
fi

# Process debug files if directory exists
if [[ -d "debug" ]]; then
    process_files "debug"
fi

echo "Combined all .h and .c files into $output_file"
