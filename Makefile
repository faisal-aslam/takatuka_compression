CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = weighted_freq
SRCS = main.c hash_table.c sequence_map.c heap.c file_processor.c
OBJS = $(SRCS:.c=.o)
HEADERS = weighted_freq.h hash_table.h sequence_map.h heap.h file_processor.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
