make clean && make debug
valgrind --leak-check=full --track-origins=yes ./compress-debug tests/testSmall_50.txt >> val.txt
