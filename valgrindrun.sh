make clean && make debug
valgrind --leak-check=full --track-origins=yes ./decompress-debug out.bin tests.txt >> val.txt
