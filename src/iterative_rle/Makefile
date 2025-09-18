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

# Pull in sources from .mk
include rle_sources.mk
SRCS := $(RLE_SRCS)

# Build directories
BUILD_DIR := ../../build
OBJ_DIR := $(BUILD_DIR)/obj

# Objects (mirror src paths under obj/)
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))

# Output binary
OUT := $(BUILD_DIR)/iterative_rle

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS)

# Pattern rule: compile .c into obj dir
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUT)
