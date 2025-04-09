#!/bin/bash
cd src/
rm -f combined_code.c
output_file="combined_code.c"

echo "// Combined C and H Files" > "$output_file"

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

# Process second_pass files

if [[ -d "second_pass" ]]; then
    echo -e "second_pass \n"
    process_files "second_pass"
fi


# Process debug files
if [[ -d "debug" ]]; then
    echo -e "debug \n"
    process_files "debug"
fi

# Process decompress files
if [[ -d "decompress" ]]; then
    echo -e "decompress \n"
    process_files "decompress"
fi

# Process write_in_file files
if [[ -d "write_in_file" ]]; then
    echo -e "write_in_file \n"
    process_files "write_in_file"
fi

echo "Combined all .h and .c files into $output_file"
