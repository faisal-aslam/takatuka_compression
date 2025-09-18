# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -O2
DEBUG_FLAGS := -g -O0 -DDEBUG

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
DEBUG_OBJ_DIR := $(BUILD_DIR)/obj_debug

# Objects (mirror src paths under obj/)
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
DEBUG_OBJS := $(patsubst %.c,$(DEBUG_OBJ_DIR)/%.o,$(SRCS))

# Output binaries
OUT := $(BUILD_DIR)/iterative_rle
DEBUG_OUT := $(BUILD_DIR)/iterative_rle_debug

.PHONY: all clean run val debug

all: $(OUT)

debug: $(DEBUG_OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) -lm

$(DEBUG_OUT): $(DEBUG_OBJS)
	$(CC) $(DEBUG_FLAGS) $(CFLAGS) $(INCLUDES) -o $@ $(DEBUG_OBJS) -lm

# Pattern rule: compile .c into obj dir (release)
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Pattern rule: compile .c into debug obj dir
$(DEBUG_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(DEBUG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: $(OUT)
	./$(OUT) $(ARGS)

run_debug: $(DEBUG_OUT)
	./$(DEBUG_OUT) $(ARGS)

val: $(OUT)
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --track-fds=yes \
	         ./$(OUT) $(ARGS)

val_debug: $(DEBUG_OUT)
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --track-fds=yes \
	         ./$(DEBUG_OUT) $(ARGS)

clean:
	rm -rf $(OBJ_DIR) $(DEBUG_OBJ_DIR) $(OUT) $(DEBUG_OUT)