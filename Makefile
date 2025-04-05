# Compiler and base flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O3 -march=native -flto -funroll-loops \
         -fomit-frame-pointer -MMD -I./src \
         -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-math-errno \
         -fstrict-aliasing -ftree-vectorize -fno-stack-protector
LDFLAGS = -flto -O3 -fuse-linker-plugin

# Targets
TARGET = weighted_freq
DEBUG_TARGET = weighted_freq-debug
COMPRESS_TARGET = compress
DECOMPRESS_TARGET = decompress

# Directory structure
SRC_DIR = src
BUILD_DIR = build
DEBUG_DIR = $(SRC_DIR)/debug
SECOND_PASS_DIR = $(SRC_DIR)/second_pass
WRITE_IN_FILE_DIR = $(SRC_DIR)/write_in_file
DECOMPRESS_DIR = $(SRC_DIR)/decompress

# Source files
MAIN_SRCS = $(wildcard $(SRC_DIR)/*.c)
SECOND_PASS_SRCS = $(wildcard $(SECOND_PASS_DIR)/*.c)
DEBUG_SRCS = $(wildcard $(DEBUG_DIR)/*.c)
WRITE_IN_FILE_SRCS = $(wildcard $(WRITE_IN_FILE_DIR)/*.c)
DECOMPRESS_SRCS = $(DECOMPRESS_DIR)/decompress.c

# Object files
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(MAIN_SRCS))
SECOND_PASS_OBJ = $(patsubst $(SECOND_PASS_DIR)/%.c,$(BUILD_DIR)/second_pass/%.o,$(SECOND_PASS_SRCS))
DEBUG_OBJS = $(patsubst $(DEBUG_DIR)/%.c,$(BUILD_DIR)/debug/%.o,$(DEBUG_SRCS))
WRITE_IN_FILE_OBJS = $(patsubst $(WRITE_IN_FILE_DIR)/%.c,$(BUILD_DIR)/write_in_file/%.o,$(WRITE_IN_FILE_SRCS))
DECOMPRESS_OBJ = $(BUILD_DIR)/decompress/decompress.o

# Dependencies
DEPS = $(MAIN_OBJS:.o=.d) $(SECOND_PASS_OBJ:.o=.d) $(DEBUG_OBJS:.o=.d) $(WRITE_IN_FILE_OBJS:.o=.d) $(DECOMPRESS_OBJ:.o=.d)

.PHONY: all clean release debug compress decompress

all: $(TARGET)

release: CFLAGS += -DNDEBUG
release: $(TARGET)

debug: CFLAGS += -DDEBUG -g
debug: $(DEBUG_TARGET)

compress: $(TARGET)
	@echo "Compression functionality is built into $(TARGET)"

decompress: $(DECOMPRESS_TARGET)

$(TARGET): $(MAIN_OBJS) $(SECOND_PASS_OBJ) $(WRITE_IN_FILE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(DEBUG_TARGET): $(MAIN_OBJS) $(SECOND_PASS_OBJ) $(DEBUG_OBJS) $(WRITE_IN_FILE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(DECOMPRESS_TARGET): $(DECOMPRESS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Main source compilation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Second pass compilation
$(BUILD_DIR)/second_pass/%.o: $(SECOND_PASS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Debug source compilation
$(BUILD_DIR)/debug/%.o: $(DEBUG_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -DDEBUG -g -c $< -o $@

# Write in file compilation
$(BUILD_DIR)/write_in_file/%.o: $(WRITE_IN_FILE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Decompress compilation
$(BUILD_DIR)/decompress/%.o: $(DECOMPRESS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(DEBUG_TARGET) $(COMPRESS_TARGET) $(DECOMPRESS_TARGET) $(BUILD_DIR) src/combined_*.c ./a.out

-include $(DEPS)
