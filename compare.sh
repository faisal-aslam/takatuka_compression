make clean && make release
time zstd tests/testText.txt
time 7z a testText.7z tests/testText.txt
time ./compress tests/testText.txt
