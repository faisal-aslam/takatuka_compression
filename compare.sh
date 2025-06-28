#!/bin/bash

make clean && make release
clear

LOG_FILE="timing.log"
echo "Run started at $(date)" | tee "$LOG_FILE"

# Remove compressed files
rm -f ./tests/testText.txt.zst
rm -f testText.7z

# Compress with zstd
echo -e "\nCompressing with zstd..." | tee -a "$LOG_FILE"
{ time zstd ./tests/testText.txt; } 2>&1 | tee -a "$LOG_FILE"

sleep 1

# Compress with 7z
echo -e "\nCompressing with 7z..." | tee -a "$LOG_FILE"
{ time 7z a testText.7z ./tests/testText.txt; } 2>&1 | tee -a "$LOG_FILE"

sleep 1

# Compress with custom compressor
echo -e "\nCompressing with custom tool..." | tee -a "$LOG_FILE"
{ time ./compress ./tests/testText.txt; } 2>&1 | tee -a "$LOG_FILE"

echo -e "\nRun completed at $(date)" | tee -a "$LOG_FILE"

