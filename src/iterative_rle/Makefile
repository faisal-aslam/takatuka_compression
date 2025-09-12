CC      = gcc
CFLAGS  = -Wall -Wextra -O2
OBJ     = main.o sort.o bitmap.o fileio.o
TARGET  = iterative_rle

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

main.o: main.c sort.h bitmap.h fileio.h
sort.o: sort.c sort.h
bitmap.o: bitmap.c bitmap.h
fileio.o: fileio.c fileio.h

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
