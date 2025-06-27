make clean && make debug
valgrind --leak-check=full --track-origins=yes ./compress-debug tests/test.bin >> 1.txt
