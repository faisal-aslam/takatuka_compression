#!/bin/bash
cd src/
rm combined_code.c
output_file="combined_code.c"
echo "// Combined C and H Files" > "$output_file"

# Append headers
for file in *.h; do
    echo -e "\n// === FILE: $file ===\n" >> "$output_file"
    cat "$file" >> "$output_file"
done

# Append source files
for file in *.c; do
    echo -e "\n// === FILE: $file ===\n" >> "$output_file"
    cat "$file" >> "$output_file"
done

echo "Combined all .h and .c files into $output_file"

