# Local Makefile for iterative_rle
# This can be called standalone or from the parent Makefile

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -O2

# Include directories
INC_DIRS := \
    .. \
    ../map \
    ../graph \
    ../graph/shortest_path \
    ../files \
    ../files/compression \
    ../files/decompression \
    ../decompress

INCLUDES := $(addprefix -I, $(INC_DIRS))

# Base sources
SRCS := main.c

# Pull in external sources
include rle_sources.mk
SRCS += $(RLE_EXTRA_SRCS)

# Objects
OBJS := $(SRCS:.c=.o)

# Output binary
OUT := ../../build/iterative_rle

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(OUT)
